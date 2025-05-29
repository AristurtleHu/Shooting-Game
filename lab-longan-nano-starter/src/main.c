#include "assembly/example.h"
#include "lcd/lcd.h"
#include "math.h"
#include "stdio.h"
#include "utils.h"

// Game Constants
#define MAX_ENEMIES 3
#define MAX_ENEMY_BULLETS 512
#define MAX_PLAYER_BULLETS 32

#define PLAYER_SPEED 2

#define ENEMY_WIDTH 10
#define ENEMY_HEIGHT 10
#define ENEMY_CENTER_OFFSET (ENEMY_WIDTH / 2)

#define PLAYER_BULLET_DRAW_SIZE 4
#define PLAYER_BULLET_SPEED 4.0f
#define PLAYER_BULLET_COOLDOWN_FRAMES 8
#define PLAYER_BULLET_CENTER_OFFSET (PLAYER_BULLET_DRAW_SIZE / 2)

#define ENEMY_BULLET_SPEED 2.5f
#define BOSS_BULLET_SPEED 3.0f

#define BULLET_CIRCLE_DRAW_SIZE 3
#define BULLET_STRAIGHT_DRAW_SIZE 4
#define BULLET_VISUAL_OFFSET 2.0f

#define ENEMY_SPAWN_INTERVAL 50
#define ENEMY_SHOOT_INTERVAL 40
#define BOSS_BULLET_SPAWN_INTERVAL 30
#define BOSS_BULLETS_PER_WAVE 60

#define BOSS_SITE_X 100
#define BOSS_SITE_Y 10
#define BOSS_SITE_WIDTH 20
#define BOSS_SITE_HEIGHT 20
#define BOSS_CENTER_X (BOSS_SITE_X + BOSS_SITE_WIDTH / 2)
#define BOSS_CENTER_Y (BOSS_SITE_Y + BOSS_SITE_HEIGHT / 2)

#define COLLISION_THRESHOLD_PLAYER_BULLET_ENEMY                                \
  (ENEMY_CENTER_OFFSET + PLAYER_BULLET_CENTER_OFFSET)

// Bullet type
typedef enum {
  BULLET_TYPE_CIRCLE,   // white, circle, straight
  BULLET_TYPE_STRAIGHT, // magenta, square, straight
  BULLET_TYPE_SINE,     // cyan, triangle, sine wave
  BULLET_TYPE_SPIRAL    // yellow, diamond, spiral
} BulletType;

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
  const int player_size = 6; // Player is a square of side player_size
  const int player_center_offset = player_size / 2;

  // Enemy structure
  typedef struct {
    int x, y, dx, dy, alive;
  } Enemy;
  Enemy enemies[MAX_ENEMIES] = {0};
  int enemy_count = 0;

  // Bullet structure (boss/enemy bullets)
  typedef struct {
    float x, y;
    float dx, dy;
    int alive;
    BulletType type;
    float t;              // time for sine/spiral
    float base_x, base_y; // for sine/spiral
    float angle;          // for spiral
  } Bullet;
  Bullet bullets[MAX_ENEMY_BULLETS] = {0};
  int bullet_count = 0;

  // Player bullet structure
  typedef struct {
    float x, y;
    float dx, dy;
    int alive;
    int target_idx; // index of target enemy
  } PlayerBullet;
  PlayerBullet player_bullets[MAX_PLAYER_BULLETS] = {0};
  int player_bullet_count = 0;
  int player_bullet_cooldown = 0;

  // Draw boss
  LCD_Fill(BOSS_SITE_X, BOSS_SITE_Y, BOSS_SITE_X + BOSS_SITE_WIDTH - 1,
           BOSS_SITE_Y + BOSS_SITE_HEIGHT - 1, YELLOW);

  while (1) {
    LCD_Clear(BLACK);

    // Handle player movement
    if (Get_Button(JOY_LEFT) && player_x > 0)
      player_x -= PLAYER_SPEED;
    if (Get_Button(JOY_RIGHT) && player_x < LCD_W - player_size)
      player_x += PLAYER_SPEED;
    if (Get_Button(JOY_UP) && player_y > 0)
      player_y -= PLAYER_SPEED;
    if (Get_Button(JOY_DOWN) && player_y < LCD_H - player_size)
      player_y += PLAYER_SPEED;

    // Player shoot (seek to nearest enemy)
    if (Get_Button(BUTTON_1) && player_bullet_cooldown == 0 &&
        player_bullet_count < MAX_PLAYER_BULLETS) {
      // Find nearest alive enemy
      int nearest_idx = -1;
      float nearest_dist_sq = 1e18f;
      for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].alive) {
          float dx_enemy = (enemies[i].x + ENEMY_CENTER_OFFSET) -
                           (player_x + player_center_offset);
          float dy_enemy = (enemies[i].y + ENEMY_CENTER_OFFSET) -
                           (player_y + player_center_offset);
          float dist_sq = dx_enemy * dx_enemy + dy_enemy * dy_enemy;
          if (dist_sq < nearest_dist_sq) {
            nearest_dist_sq = dist_sq;
            nearest_idx = i;
          }
        }
      }
      for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (!player_bullets[i].alive) {
          float px_center = player_x + player_center_offset;
          float py_center = player_y + player_center_offset;
          float dx_bullet, dy_bullet, len_bullet;
          if (nearest_idx != -1) {
            float tx_center = enemies[nearest_idx].x + ENEMY_CENTER_OFFSET;
            float ty_center = enemies[nearest_idx].y + ENEMY_CENTER_OFFSET;
            dx_bullet = tx_center - px_center;
            dy_bullet = ty_center - py_center;
            len_bullet = sqrtf(dx_bullet * dx_bullet + dy_bullet * dy_bullet);
            if (len_bullet < 1e-3f)
              len_bullet = 1.0f; // Avoid division by zero
            player_bullets[i].target_idx = nearest_idx;
          } else {
            // No enemy: shoot straight up
            dx_bullet = 0.0f;
            dy_bullet = -1.0f;
            len_bullet = 1.0f;
            player_bullets[i].target_idx = -1;
          }
          player_bullets[i].x = px_center - PLAYER_BULLET_CENTER_OFFSET;
          player_bullets[i].y = py_center - PLAYER_BULLET_CENTER_OFFSET;
          player_bullets[i].dx = PLAYER_BULLET_SPEED * dx_bullet / len_bullet;
          player_bullets[i].dy = PLAYER_BULLET_SPEED * dy_bullet / len_bullet;
          player_bullets[i].alive = 1;
          player_bullet_count++;
          player_bullet_cooldown = PLAYER_BULLET_COOLDOWN_FRAMES;
          break;
        }
      }
    }
    if (player_bullet_cooldown > 0)
      player_bullet_cooldown--;

    // Spawn enemies by time
    static int enemy_spawn_timer = 0;
    enemy_spawn_timer++;
    if (enemy_spawn_timer > ENEMY_SPAWN_INTERVAL && enemy_count < MAX_ENEMIES) {
      for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive) {
          enemies[i].x = rand() % (LCD_W - ENEMY_WIDTH);
          enemies[i].y = rand() % (LCD_H - ENEMY_HEIGHT);
          enemies[i].dx = (rand() % 2) ? 1 : -1;
          enemies[i].dy = (rand() % 2) ? 1 : -1;
          enemies[i].alive = 1;
          enemy_count++;
          break;
        }
      }
      enemy_spawn_timer = 0;
    }

    // Enemey shooting
    static int enemy_shoot_timer = 0;
    enemy_shoot_timer++;
    if (enemy_shoot_timer > ENEMY_SHOOT_INTERVAL) {
      for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].alive && bullet_count < MAX_ENEMY_BULLETS) {
          for (int j = 0; j < MAX_ENEMY_BULLETS; ++j) {
            if (!bullets[j].alive) {
              float ex_center = enemies[i].x + ENEMY_CENTER_OFFSET;
              float ey_center = enemies[i].y + ENEMY_CENTER_OFFSET;
              float px_center = player_x + player_center_offset;
              float py_center = player_y + player_center_offset;
              float dx_bullet = px_center - ex_center;
              float dy_bullet = py_center - ey_center;
              float len_bullet =
                  sqrtf(dx_bullet * dx_bullet + dy_bullet * dy_bullet);
              if (len_bullet < 1e-3f)
                len_bullet = 1.0f;

              bullets[j].x =
                  ex_center - BULLET_VISUAL_OFFSET; // Approximate center
              bullets[j].y = ey_center - BULLET_VISUAL_OFFSET;
              bullets[j].dx = ENEMY_BULLET_SPEED * dx_bullet / len_bullet;
              bullets[j].dy = ENEMY_BULLET_SPEED * dy_bullet / len_bullet;
              bullets[j].alive = 1;
              bullets[j].type =
                  BULLET_TYPE_STRAIGHT; // Enemy shoots straight bullets
              bullets[j].t = 0;
              bullet_count++;
              break;
            }
          }
        }
      }
      enemy_shoot_timer = 0;
    }

    // Update and draw enemies
    for (int i = 0; i < MAX_ENEMIES; ++i) {
      if (enemies[i].alive) {
        enemies[i].x += enemies[i].dx;
        enemies[i].y += enemies[i].dy;
        if (enemies[i].x <= 0 || enemies[i].x >= LCD_W - ENEMY_WIDTH)
          enemies[i].dx = -enemies[i].dx;
        if (enemies[i].y <= 0 || enemies[i].y >= LCD_H - ENEMY_HEIGHT)
          enemies[i].dy = -enemies[i].dy;
        LCD_Fill(enemies[i].x, enemies[i].y, enemies[i].x + ENEMY_WIDTH - 1,
                 enemies[i].y + ENEMY_HEIGHT - 1, GREEN);
      }
    }

    // Bullet boom: spawn a lot of bullets in all directions from boss site
    static int bullet_spawn_timer = 0;
    bullet_spawn_timer++;
    if (bullet_spawn_timer > BOSS_BULLET_SPAWN_INTERVAL &&
        bullet_count < MAX_ENEMY_BULLETS) {
      float angle_step = 2 * 3.1415926f / BOSS_BULLETS_PER_WAVE;
      for (int b = 0;
           b < BOSS_BULLETS_PER_WAVE && bullet_count < MAX_ENEMY_BULLETS; ++b) {
        for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
          if (!bullets[i].alive) {
            float angle = b * angle_step;
            bullets[i].x = BOSS_CENTER_X - BULLET_VISUAL_OFFSET;
            bullets[i].y = BOSS_CENTER_Y - BULLET_VISUAL_OFFSET;
            bullets[i].dx = BOSS_BULLET_SPEED * cosf(angle);
            bullets[i].dy = BOSS_BULLET_SPEED * sinf(angle);

            if (b % 3 == 0) {
              bullets[i].type = BULLET_TYPE_CIRCLE;
            } else if (b % 3 == 1) {
              bullets[i].type = BULLET_TYPE_SINE;
              bullets[i].base_x =
                  bullets[i].x; // Store initial position for sine path
              bullets[i].base_y = bullets[i].y;
              bullets[i].t = 0;
              bullets[i].angle = angle; // Store main direction angle
            } else {
              bullets[i].type = BULLET_TYPE_SPIRAL;
              bullets[i].base_x =
                  BOSS_CENTER_X; // Spiral originates from boss center
              bullets[i].base_y = BOSS_CENTER_Y;
              bullets[i].t = 0;
              bullets[i].angle = angle; // Initial angle for spiral
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
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
      if (bullets[i].alive) {
        if (bullets[i].type == BULLET_TYPE_CIRCLE ||
            bullets[i].type == BULLET_TYPE_STRAIGHT) {
          bullets[i].x += bullets[i].dx;
          bullets[i].y += bullets[i].dy;
        } else if (bullets[i].type == BULLET_TYPE_SINE) {
          bullets[i].t += 1.0f;
          float freq = 0.15f;
          float amp = 12.0f;
          // Main direction vector
          float main_dir_x = cosf(bullets[i].angle);
          float main_dir_y = sinf(bullets[i].angle);
          // Position along the main direction
          float path_x =
              bullets[i].base_x + main_dir_x * BOSS_BULLET_SPEED * bullets[i].t;
          float path_y =
              bullets[i].base_y + main_dir_y * BOSS_BULLET_SPEED * bullets[i].t;
          // Perpendicular vector for oscillation
          float perp_x = -main_dir_y;
          float perp_y = main_dir_x;
          float offset = amp * sinf(bullets[i].t * freq);
          bullets[i].x = path_x + perp_x * offset;
          bullets[i].y = path_y + perp_y * offset;
        } else if (bullets[i].type == BULLET_TYPE_SPIRAL) {
          bullets[i].t += 1.0f;
          float spiral_growth_rate = 0.7f;
          float spiral_angular_speed = 0.12f;
          float current_radius =
              10.0f +
              bullets[i].t * spiral_growth_rate; // Radius increases over time
          float current_angle =
              bullets[i].angle +
              bullets[i].t * spiral_angular_speed; // Angle rotates over time
          bullets[i].x = bullets[i].base_x +
                         current_radius * cosf(current_angle) -
                         BULLET_VISUAL_OFFSET;
          bullets[i].y = bullets[i].base_y +
                         current_radius * sinf(current_angle) -
                         BULLET_VISUAL_OFFSET;
        }

        // Out of bounds check
        if (bullets[i].x < -BULLET_STRAIGHT_DRAW_SIZE || bullets[i].x > LCD_W ||
            bullets[i].y < -BULLET_STRAIGHT_DRAW_SIZE || bullets[i].y > LCD_H) {
          bullets[i].alive = 0;
          bullet_count--;
          continue;
        }

        // Draw with different shape and color
        int bx_int = (int)bullets[i].x;
        int by_int = (int)bullets[i].y;
        if (bullets[i].type == BULLET_TYPE_CIRCLE) {
          LCD_Fill(bx_int, by_int, bx_int + BULLET_CIRCLE_DRAW_SIZE - 1,
                   by_int + BULLET_CIRCLE_DRAW_SIZE - 1, WHITE);
        } else if (bullets[i].type == BULLET_TYPE_STRAIGHT) {
          LCD_Fill(bx_int, by_int, bx_int + BULLET_STRAIGHT_DRAW_SIZE - 1,
                   by_int + BULLET_STRAIGHT_DRAW_SIZE - 1, MAGENTA);
        } else if (bullets[i].type == BULLET_TYPE_SINE) {
          int tri_x[3] = {bx_int + 2, bx_int,
                          bx_int + 4}; // Centered around (bx+2, by+2) approx
          int tri_y[3] = {by_int, by_int + 4, by_int + 4};
          LCD_DrawLine(tri_x[0], tri_y[0], tri_x[1], tri_y[1], CYAN);
          LCD_DrawLine(tri_x[1], tri_y[1], tri_x[2], tri_y[2], CYAN);
          LCD_DrawLine(tri_x[2], tri_y[2], tri_x[0], tri_y[0], CYAN);
        } else if (bullets[i].type == BULLET_TYPE_SPIRAL) {
          int diamond_cx = bx_int + 2; // Approx center for a 5x5 diamond
          int diamond_cy = by_int + 2;
          LCD_DrawLine(diamond_cx, diamond_cy - 3, diamond_cx + 3, diamond_cy,
                       YELLOW);
          LCD_DrawLine(diamond_cx + 3, diamond_cy, diamond_cx, diamond_cy + 3,
                       YELLOW);
          LCD_DrawLine(diamond_cx, diamond_cy + 3, diamond_cx - 3, diamond_cy,
                       YELLOW);
          LCD_DrawLine(diamond_cx - 3, diamond_cy, diamond_cx, diamond_cy - 3,
                       YELLOW);
        }
      }
    }

    // Update and draw player bullets
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
      if (player_bullets[i].alive) {
        player_bullets[i].x += player_bullets[i].dx;
        player_bullets[i].y += player_bullets[i].dy;
        int hit = 0;

        int target_enemy_idx = player_bullets[i].target_idx;
        if (target_enemy_idx >= 0 && target_enemy_idx < MAX_ENEMIES &&
            enemies[target_enemy_idx].alive) {
          float p_bullet_cx = player_bullets[i].x + PLAYER_BULLET_CENTER_OFFSET;
          float p_bullet_cy = player_bullets[i].y + PLAYER_BULLET_CENTER_OFFSET;
          float enemy_cx = enemies[target_enemy_idx].x + ENEMY_CENTER_OFFSET;
          float enemy_cy = enemies[target_enemy_idx].y + ENEMY_CENTER_OFFSET;

          // Re-target (seek)
          float dx_seek = enemy_cx - p_bullet_cx;
          float dy_seek = enemy_cy - p_bullet_cy;
          float len_seek = sqrtf(dx_seek * dx_seek + dy_seek * dy_seek);
          if (len_seek > 1.0f) { // Only adjust if not too close
            player_bullets[i].dx = PLAYER_BULLET_SPEED * dx_seek / len_seek;
            player_bullets[i].dy = PLAYER_BULLET_SPEED * dy_seek / len_seek;
          }

          // Collision check
          if (fabsf(p_bullet_cx - enemy_cx) <
                  COLLISION_THRESHOLD_PLAYER_BULLET_ENEMY &&
              fabsf(p_bullet_cy - enemy_cy) <
                  COLLISION_THRESHOLD_PLAYER_BULLET_ENEMY) {
            enemies[target_enemy_idx].alive = 0;
            enemy_count--;
            player_bullets[i].alive = 0;
            player_bullet_count--;
            hit = 1;
          }
        }
        if (!hit) {
          // Out of screen
          if (player_bullets[i].x < -PLAYER_BULLET_DRAW_SIZE ||
              player_bullets[i].x > LCD_W ||
              player_bullets[i].y < -PLAYER_BULLET_DRAW_SIZE ||
              player_bullets[i].y > LCD_H) {
            player_bullets[i].alive = 0;
            player_bullet_count--;
            continue;
          }
          LCD_Fill((int)player_bullets[i].x, (int)player_bullets[i].y,
                   (int)player_bullets[i].x + PLAYER_BULLET_DRAW_SIZE - 1,
                   (int)player_bullets[i].y + PLAYER_BULLET_DRAW_SIZE - 1,
                   BLUE);
        }
      }
    }

    // Draw player
    LCD_Fill(player_x, player_y, player_x + player_size - 1,
             player_y + player_size - 1, RED);

    // Draw boss site as a yellow square (static)
    LCD_Fill(BOSS_SITE_X, BOSS_SITE_Y, BOSS_SITE_X + BOSS_SITE_WIDTH - 1,
             BOSS_SITE_Y + BOSS_SITE_HEIGHT - 1, YELLOW);

    // FPS and Entity count (optional, can be uncommented if snprintf and
    // LCD_ShowString are robust) static int frame_count = 0; static uint32_t
    // last_time = 0; uint32_t current_time = systick_current_value(); //
    // Assuming systick or similar timer frame_count++; if (current_time -
    // last_time >= 1000) { // Update FPS every second (approx)
    //    fps = frame_count;
    //    frame_count = 0;
    //    last_time = current_time;
    // }
    // char info[32];
    // int current_entity_count = enemy_count + bullet_count +
    // player_bullet_count + 1; // +1 for player snprintf(info, sizeof(info),
    // "FPS:%d ENT:%d", fps, current_entity_count); LCD_ShowString(2, 2, (u8
    // *)info, YELLOW);

    delay_1ms(20); // Frame delay
  }
}
