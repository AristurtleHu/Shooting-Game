#include "assembly/example.h"
#include "lcd/lcd.h"
#include "math.h"
#include "stdio.h"
#include "utils.h"

void Inp_init(void) {
  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_GPIOC);

  gpio_init(GPIOA, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ,
            GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
  gpio_init(GPIOC, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ,
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
}

void IO_init(void) {
  Inp_init(); // inport init
  Lcd_Init(); // LCD init
}

void Board_self_test(void) {
  while (1) {
    LCD_ShowString(60, 25, (u8 *)"TEST (25s)", WHITE);
    if (Get_Button(JOY_LEFT)) {
      LCD_ShowString(5, 25, (u8 *)"L", BLUE);
    }
    if (Get_Button(JOY_DOWN)) {
      LCD_ShowString(25, 45, (u8 *)"D", BLUE);
      LCD_ShowString(60, 25, (u8 *)"TEST", GREEN);
    }
    if (Get_Button(JOY_UP)) {
      LCD_ShowString(25, 5, (u8 *)"U", BLUE);
    }
    if (Get_Button(JOY_RIGHT)) {
      LCD_ShowString(45, 25, (u8 *)"R", BLUE);
    }
    if (Get_Button(JOY_CTR)) {
      LCD_ShowString(25, 25, (u8 *)"C", BLUE);
    }
    if (Get_Button(BUTTON_1)) {
      LCD_ShowString(60, 5, (u8 *)"SW1", BLUE);
    }
    if (Get_Button(BUTTON_2)) {
      LCD_ShowString(60, 45, (u8 *)"SW2", BLUE);
    }
    delay_1ms(10);
    LCD_Clear(BLACK);
  }
}

int main(void) {
  IO_init();
  LCD_Clear(BLACK);
  start();
  // game start

  // Player position
  int player_x = 30, player_y = 30;
  const int player_size = 6;

  // Enemy structure
  typedef struct {
    int x, y, dx, dy, alive;
  } Enemy;
  Enemy enemies[5] = {0};
  int enemy_count = 0;
  int enemy_spawn_timer = 0;

  // Bullet structure (boss/enemy bullets)
  typedef struct {
    float x, y;
    float dx, dy;
    int alive;
    int type;
    float t;              // time for sine/spiral
    float base_x, base_y; // for sine/spiral
    float angle;          // for spiral
  } Bullet;
  Bullet bullets[1024] = {0};
  int bullet_count = 0;
  int bullet_spawn_timer = 0;

  // Player bullet structure
  typedef struct {
    float x, y;
    float dx, dy;
    int alive;
    int target_idx; // index of target enemy
  } PlayerBullet;
  PlayerBullet player_bullets[32] = {0};
  int player_bullet_count = 0;
  int player_bullet_cooldown = 0;

  // Draw boss site as a yellow square (static)
  LCD_Fill(100, 10, 120, 30, YELLOW);

// Bullet type constants
#define BULLET_CIRCLE 1   // white, circle, straight
#define BULLET_STRAIGHT 2 // magenta, square, straight
#define BULLET_SINE 3     // cyan, triangle, sine wave
#define BULLET_SPIRAL 4   // yellow, diamond, spiral

  while (1) {
    // Clear previous frame
    LCD_Clear(BLACK);

    // Handle player movement
    if (Get_Button(JOY_LEFT) && player_x > 0)
      player_x -= 2;
    if (Get_Button(JOY_RIGHT) && player_x < LCD_W - player_size)
      player_x += 2;
    if (Get_Button(JOY_UP) && player_y > 0)
      player_y -= 2;
    if (Get_Button(JOY_DOWN) && player_y < LCD_H - player_size)
      player_y += 2;

    // Player shoot (seek to nearest enemy)
    if (Get_Button(BUTTON_1) && player_bullet_cooldown == 0 &&
        player_bullet_count < 32) {
      // Find nearest alive enemy
      int nearest_idx = -1;
      float nearest_dist = 1e9;
      for (int i = 0; i < 3; ++i) {
        if (enemies[i].alive) {
          float dx = (enemies[i].x + 5) - (player_x + player_size / 2);
          float dy = (enemies[i].y + 5) - (player_y + player_size / 2);
          float dist = dx * dx + dy * dy;
          if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_idx = i;
          }
        }
      }
      for (int i = 0; i < 32; ++i) {
        if (!player_bullets[i].alive) {
          float px = player_x + player_size / 2;
          float py = player_y + player_size / 2;
          float dx, dy, len, speed = 4.0f;
          if (nearest_idx != -1) {
            float tx = enemies[nearest_idx].x + 5;
            float ty = enemies[nearest_idx].y + 5;
            dx = tx - px;
            dy = ty - py;
            len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-3)
              len = 1.0f;
            player_bullets[i].target_idx = nearest_idx;
          } else {
            // No enemy: shoot straight up
            dx = 0.0f;
            dy = -1.0f;
            len = 1.0f;
            player_bullets[i].target_idx = -1;
          }
          player_bullets[i].x = px - 2;
          player_bullets[i].y = py - 2;
          player_bullets[i].dx = speed * dx / len;
          player_bullets[i].dy = speed * dy / len;
          player_bullets[i].alive = 1;
          player_bullet_count++;
          player_bullet_cooldown = 8; // cooldown frames
          break;
        }
      }
    }
    if (player_bullet_cooldown > 0)
      player_bullet_cooldown--;

    // Spawn enemies by time, at most 3
    enemy_spawn_timer++;
    if (enemy_spawn_timer > 50 && enemy_count < 3) {
      for (int i = 0; i < 3; ++i) {
        if (!enemies[i].alive) {
          enemies[i].x = rand() % (LCD_W - 10);
          enemies[i].y = rand() % (LCD_H - 10);
          enemies[i].dx = (rand() % 2) ? 1 : -1;
          enemies[i].dy = (rand() % 2) ? 1 : -1;
          enemies[i].alive = 1;
          enemy_count++;
          break;
        }
      }
      enemy_spawn_timer = 0;
    }

    // --- ENEMY SHOOTING LOGIC ---
    // Each enemy can shoot at the player every N frames
    static int enemy_shoot_timer = 0;
    enemy_shoot_timer++;
    if (enemy_shoot_timer > 40) { // adjust shoot interval as needed
      for (int i = 0; i < 3; ++i) {
        if (enemies[i].alive && bullet_count < 1024) {
          // Find a free bullet slot
          for (int j = 0; j < 1024; ++j) {
            if (!bullets[j].alive) {
              float ex = enemies[i].x + 5;
              float ey = enemies[i].y + 5;
              float px = player_x + player_size / 2;
              float py = player_y + player_size / 2;
              float dx = px - ex;
              float dy = py - ey;
              float len = sqrtf(dx * dx + dy * dy);
              float speed = 2.5f;
              if (len < 1e-3)
                len = 1.0f;
              bullets[j].x = ex - 2;
              bullets[j].y = ey - 2;
              bullets[j].dx = speed * dx / len;
              bullets[j].dy = speed * dy / len;
              bullets[j].alive = 1;
              bullets[j].type = BULLET_STRAIGHT;
              bullets[j].t = 0;
              bullet_count++;
              break;
            }
          }
        }
      }
      enemy_shoot_timer = 0;
    }
    // --- END ENEMY SHOOTING LOGIC ---

    // Update and draw enemies
    for (int i = 0; i < 3; ++i) {
      if (enemies[i].alive) {
        enemies[i].x += enemies[i].dx;
        enemies[i].y += enemies[i].dy;
        if (enemies[i].x <= 0 || enemies[i].x >= LCD_W - 10)
          enemies[i].dx = -enemies[i].dx;
        if (enemies[i].y <= 0 || enemies[i].y >= LCD_H - 10)
          enemies[i].dy = -enemies[i].dy;
        LCD_Fill(enemies[i].x, enemies[i].y, enemies[i].x + 9, enemies[i].y + 9,
                 GREEN);
      }
    }

    // Bullet boom: spawn a lot of bullets in all directions from boss site
    bullet_spawn_timer++;
    if (bullet_spawn_timer > 30 && bullet_count < 1024) {
      int bullets_per_boom = 60; // number of bullets per boom
      float angle_step = 2 * 3.1415926f / bullets_per_boom;
      float speed = 3.0f;
      for (int b = 0; b < bullets_per_boom && bullet_count < 1024; ++b) {
        for (int i = 0; i < 1024; ++i) {
          if (!bullets[i].alive) {
            float angle = b * angle_step;
            // Boss site center: (110, 20)
            bullets[i].x = 110.0f - 2.0f;
            bullets[i].y = 20.0f - 2.0f;
            bullets[i].dx = speed * cosf(angle);
            bullets[i].dy = speed * sinf(angle);
            // Alternate bullet types for variety
            if (b % 3 == 0) {
              bullets[i].type = BULLET_CIRCLE; // straight, circle
            } else if (b % 3 == 1) {
              bullets[i].type = BULLET_SINE; // sine, triangle
              bullets[i].base_x = bullets[i].x;
              bullets[i].base_y = bullets[i].y;
              bullets[i].t = 0;
              bullets[i].angle = angle;
            } else {
              bullets[i].type = BULLET_SPIRAL; // spiral, diamond
              bullets[i].base_x = bullets[i].x;
              bullets[i].base_y = bullets[i].y;
              bullets[i].t = 0;
              bullets[i].angle = angle;
            }
            bullets[i].alive = 1;
            bullet_count++;
            break;
          }
        }
      }
      bullet_spawn_timer = 0;
    }

    // Update and draw boss and enemy bullets
    for (int i = 0; i < 1024; ++i) {
      if (bullets[i].alive) {
        // Trajectory update by type
        if (bullets[i].type == BULLET_CIRCLE ||
            bullets[i].type == BULLET_STRAIGHT) {
          // Straight line
          bullets[i].x += bullets[i].dx;
          bullets[i].y += bullets[i].dy;
        } else if (bullets[i].type == BULLET_SINE) {
          // Sine wave: move forward, oscillate perpendicular
          bullets[i].t += 1.0f;
          float freq = 0.15f;
          float amp = 12.0f;
          float vx = cosf(bullets[i].angle);
          float vy = sinf(bullets[i].angle);
          float px = bullets[i].base_x + bullets[i].dx * bullets[i].t;
          float py = bullets[i].base_y + bullets[i].dy * bullets[i].t;
          // Perpendicular vector
          float perp_x = -vy;
          float perp_y = vx;
          float offset = amp * sinf(bullets[i].t * freq);
          bullets[i].x = px + perp_x * offset;
          bullets[i].y = py + perp_y * offset;
        } else if (bullets[i].type == BULLET_SPIRAL) {
          // Spiral: radius increases, angle increases
          bullets[i].t += 1.0f;
          float spiral_speed = 2.0f;
          float spiral_radius = 10.0f + bullets[i].t * 0.7f;
          float spiral_angle = bullets[i].angle + bullets[i].t * 0.12f;
          bullets[i].x = 110.0f + spiral_radius * cosf(spiral_angle) - 2.0f;
          bullets[i].y = 20.0f + spiral_radius * sinf(spiral_angle) - 2.0f;
        }

        // Out of bounds check
        if (bullets[i].x < 0 || bullets[i].x > LCD_W || bullets[i].y > LCD_H ||
            bullets[i].y < 0) {
          bullets[i].alive = 0;
          bullet_count--;
          continue;
        }

        // Draw with different shape and color
        if (bullets[i].type == BULLET_CIRCLE) {
          // White circle
          LCD_Fill((int)bullets[i].x, (int)bullets[i].y, (int)bullets[i].x + 2,
                   (int)bullets[i].y + 2, WHITE);
        } else if (bullets[i].type == BULLET_STRAIGHT) {
          // Magenta square
          LCD_Fill((int)bullets[i].x, (int)bullets[i].y, (int)bullets[i].x + 3,
                   (int)bullets[i].y + 3, MAGENTA);
        } else if (bullets[i].type == BULLET_SINE) {
          // Cyan triangle
          int bx = (int)bullets[i].x, by = (int)bullets[i].y;
          int tri_x[3] = {bx + 2, bx, bx + 4};
          int tri_y[3] = {by, by + 4, by + 4};
          LCD_DrawLine(tri_x[0], tri_y[0], tri_x[1], tri_y[1], CYAN);
          LCD_DrawLine(tri_x[1], tri_y[1], tri_x[2], tri_y[2], CYAN);
          LCD_DrawLine(tri_x[2], tri_y[2], tri_x[0], tri_y[0], CYAN);
        } else if (bullets[i].type == BULLET_SPIRAL) {
          // Yellow diamond
          int bx = (int)bullets[i].x + 2, by = (int)bullets[i].y + 2;
          LCD_DrawLine(bx, by - 3, bx + 3, by, YELLOW);
          LCD_DrawLine(bx + 3, by, bx, by + 3, YELLOW);
          LCD_DrawLine(bx, by + 3, bx - 3, by, YELLOW);
          LCD_DrawLine(bx - 3, by, bx, by - 3, YELLOW);
        }
      }
    }

    // Update and draw player bullets
    for (int i = 0; i < 32; ++i) {
      if (player_bullets[i].alive) {
        player_bullets[i].x += player_bullets[i].dx;
        player_bullets[i].y += player_bullets[i].dy;
        int hit = 0;
        int tx = -1, ty = -1;
        int idx = player_bullets[i].target_idx;
        // Only seek if there is a valid target
        if (idx >= 0 && idx < 3 && enemies[idx].alive) {
          tx = enemies[idx].x + 5;
          ty = enemies[idx].y + 5;
          float px = player_bullets[i].x + 2;
          float py = player_bullets[i].y + 2;
          float dx = tx - px;
          float dy = ty - py;
          float len = sqrtf(dx * dx + dy * dy);
          if (len > 1.0f) {
            float speed = 4.0f;
            player_bullets[i].dx = speed * dx / len;
            player_bullets[i].dy = speed * dy / len;
          }
          // Collision check
          if (fabsf(px - tx) < 7 && fabsf(py - ty) < 7) {
            enemies[idx].alive = 0;
            enemy_count--;
            player_bullets[i].alive = 0;
            player_bullet_count--;
            hit = 1;
          }
        }
        if (!hit) {
          // Out of screen
          if (player_bullets[i].x < 0 || player_bullets[i].x > LCD_W ||
              player_bullets[i].y > LCD_H || player_bullets[i].y < 0) {
            player_bullets[i].alive = 0;
            player_bullet_count--;
            continue;
          }
          LCD_Fill((int)player_bullets[i].x, (int)player_bullets[i].y,
                   (int)player_bullets[i].x + 3, (int)player_bullets[i].y + 3,
                   BLUE);
        }
      }
    }

    // Draw player
    LCD_Fill(player_x, player_y, player_x + player_size, player_y + player_size,
             RED);

    // Draw boss site as a yellow square (static)
    LCD_Fill(100, 10, 120, 30, YELLOW);

    // Entity counter
    // int entity_count =
    //   enemy_count + bullet_count + player_bullet_count + 1; // +1 for player

    // Show FPS and entity count
    // char info[32];
    // snprintf(info, sizeof(info), "FPS:%d ENT:%d", fps, entity_count);
    // LCD_ShowString(2, 2, (u8 *)info, YELLOW);

    delay_1ms(20);
  }
}
