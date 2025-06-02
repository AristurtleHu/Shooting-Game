#include "assembly/example.h"
#include "lcd/lcd.h"
#include "math.h"
#include "stdio.h"
#include "utils.h"

// Helper macro
#define min(a, b) (((a) < (b)) ? (a) : (b))

// Game Constants
#define MAX_ENEMIES 3
#define MAX_BOSS_BULLETS 20          // Bullets spawned by the central boss site
#define MAX_REGULAR_ENEMY_BULLETS 35 // Bullets spawned by enemies
#define MAX_PLAYER_BULLETS 300

#define PLAYER_SPEED 3

#define ENEMY_WIDTH 4
#define ENEMY_HEIGHT 4
#define ENEMY_CENTER_OFFSET (ENEMY_WIDTH / 2)

#define PLAYER_BULLET_DRAW_SIZE 4
#define PLAYER_BULLET_SPEED 4.5f
#define PLAYER_BULLET_COOLDOWN_FRAMES 8
#define PLAYER_BULLET_CENTER_OFFSET (PLAYER_BULLET_DRAW_SIZE / 2)

#define ENEMY_BULLET_SPEED 2.5f // Speed for regular enemy bullets
#define BOSS_BULLET_SPEED 3.5f  // Speed for boss bullets

#define BULLET_CIRCLE_DRAW_SIZE 3
#define BULLET_STRAIGHT_DRAW_SIZE 4
#define BULLET_VISUAL_OFFSET 2.0f // Offset for drawing to center the visual

#define ENEMY_SPAWN_INTERVAL 50
#define ENEMY_SHOOT_INTERVAL 20
#define BOSS_BULLET_SPAWN_INTERVAL 30
#define BOSS_BULLETS_PER_WAVE                                                  \
  MAX_BOSS_BULLETS // Number of directions in a boss wave attempt

#define BOSS_SITE_WIDTH 12
#define BOSS_SITE_HEIGHT 12
#define BOSS_SITE_X ((LCD_W - BOSS_SITE_WIDTH) / 2)
#define BOSS_SITE_Y ((LCD_H - BOSS_SITE_HEIGHT) / 2)
#define BOSS_CENTER_X (BOSS_SITE_X + BOSS_SITE_WIDTH / 2)
#define BOSS_CENTER_Y (BOSS_SITE_Y + BOSS_SITE_HEIGHT / 2)

#define COLLISION_THRESHOLD_PLAYER_BULLET_ENEMY                                \
  (ENEMY_CENTER_OFFSET + PLAYER_BULLET_CENTER_OFFSET)

#define BUTTON_ACTION_COOLDOWN_TICKS                                           \
  ((SystemCoreClock / 4 / 1000) * 300) // 300 ms in timer ticks

// Bullet type
typedef enum {
  BULLET_TYPE_CIRCLE,   // white, circle, straight
  BULLET_TYPE_STRAIGHT, // magenta, square, straight
  BULLET_TYPE_SINE,     // cyan, T, sine wave
  BULLET_TYPE_SPIRAL    // yellow, line(4 lines a group), spiral
} BulletType;

// Enemy type
typedef enum {
  ENEMY_TYPE_NORMAL,         // Green, shoots straight bullets
  ENEMY_TYPE_SINE_SHOOTER,   // Cyan, shoots sine bullets
  ENEMY_TYPE_SPIRAL_SHOOTER, // Magenta, shoots spiral bullets
} EnemyType;

// Enemy structure
typedef struct {
  int x, y, dx, dy, alive;
  int prev_x, prev_y;
  int prev_alive;
  EnemyType type;
} Enemy;

// Boss Bullet structure
typedef struct {
  float x, y;
  int alive;
  float prev_x, prev_y;
  int prev_alive;
  float t;              // time for spiral
  float base_x, base_y; // origin for spiral path
  float angle;          // main direction for sine, initial angle for spiral
} BossBullet;

// Enemy Bullet structure
typedef struct {
  float x, y;
  float dx, dy;
  int alive;
  float prev_x, prev_y;
  int prev_alive;
  BulletType type;
  float t;
  float base_x, base_y;
  float angle;
  float path_speed;
} EnemyBullet;

// Player bullet structure
typedef struct {
  float x, y;
  float dx, dy;
  int alive;
  float prev_x, prev_y;
  int prev_alive;
  int target_idx;
} PlayerBullet;

int player_x, player_y;
int prev_player_x, prev_player_y;
int player_size;
int player_center_offset;

Enemy enemies[MAX_ENEMIES];
int enemy_count;
int enemy_spawn_timer;
int enemy_shoot_timer;

BossBullet boss_bullets[MAX_BOSS_BULLETS];
int boss_bullet_count;
int boss_bullet_spawn_timer;

EnemyBullet enemy_bullets[MAX_REGULAR_ENEMY_BULLETS];
int enemy_bullet_count;

PlayerBullet player_bullets[MAX_PLAYER_BULLETS];
int player_bullet_count;
int player_bullet_cooldown;

#define MANY_BULLET 270
EnemyBullet bullets[MANY_BULLET];
int many_bullets_count;

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

void player_shoot(void) {
  if (player_bullet_cooldown == 0 && player_bullet_count < MAX_PLAYER_BULLETS) {
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
}

void spawn_enemies(void) {
  enemy_spawn_timer++;
  if (enemy_spawn_timer > ENEMY_SPAWN_INTERVAL && enemy_count < MAX_ENEMIES) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
      if (!enemies[i].alive) {
        enemies[i].x = rand() % (LCD_W - ENEMY_WIDTH);
        enemies[i].y = rand() % (LCD_H - ENEMY_HEIGHT) + 2;
        if (enemies[i].y > LCD_H - ENEMY_HEIGHT - 2)
          enemies[i].y -= ENEMY_HEIGHT;
        enemies[i].dx = (rand() % 2) ? 3 : -3;
        enemies[i].dy = (rand() % 2) ? 3 : -3;
        enemies[i].alive = 1;
        enemies[i].type = i;
        enemy_count++;
        break;
      }
    }
    enemy_spawn_timer = 0;
  }
}

void enemies_shoot(void) {
  enemy_shoot_timer++;
  if (enemy_shoot_timer > ENEMY_SHOOT_INTERVAL) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
      if (enemies[i].alive && enemy_bullet_count < MAX_REGULAR_ENEMY_BULLETS) {
        for (int j = 0; j < MAX_REGULAR_ENEMY_BULLETS; ++j) {
          if (!enemy_bullets[j].alive) {
            float ex_center = enemies[i].x + ENEMY_CENTER_OFFSET;
            float ey_center = enemies[i].y + ENEMY_CENTER_OFFSET;
            float dx_bullet = 1;
            float dy_bullet = 0.2f;
            float len_bullet =
                sqrtf(dx_bullet * dx_bullet + dy_bullet * dy_bullet);
            if (len_bullet < 1e-3f)
              len_bullet = 1.0f;

            enemy_bullets[j].x = ex_center - BULLET_VISUAL_OFFSET;
            enemy_bullets[j].y = ey_center - BULLET_VISUAL_OFFSET;
            enemy_bullets[j].alive = 1;
            enemy_bullets[j].t = 0;
            enemy_bullets[j].path_speed = 0.0f;

            switch (enemies[i].type) {
            case ENEMY_TYPE_NORMAL:
              enemy_bullets[j].type = BULLET_TYPE_STRAIGHT;
              enemy_bullets[j].dx = ENEMY_BULLET_SPEED * dx_bullet / len_bullet;
              enemy_bullets[j].dy = ENEMY_BULLET_SPEED * dy_bullet / len_bullet;
              break;
            case ENEMY_TYPE_SINE_SHOOTER:
              enemy_bullets[j].type = BULLET_TYPE_SINE;
              enemy_bullets[j].base_x =
                  enemy_bullets[j].x; // Initial pos for sine path
              enemy_bullets[j].base_y = enemy_bullets[j].y;
              enemy_bullets[j].angle = atan2f(dy_bullet, dx_bullet);
              enemy_bullets[j].path_speed = ENEMY_BULLET_SPEED;
              enemy_bullets[j].dx = 0;
              enemy_bullets[j].dy = 0;
              break;
            case ENEMY_TYPE_SPIRAL_SHOOTER:
              enemy_bullets[j].type = BULLET_TYPE_SPIRAL;
              enemy_bullets[j].base_x = ex_center; // Spiral from enemy center
              enemy_bullets[j].base_y = ey_center;
              enemy_bullets[j].angle = atan2f(dy_bullet, dx_bullet);
              enemy_bullets[j].dx = 0;
              enemy_bullets[j].dy = 0;
              break;
            }
            enemy_bullet_count++;
            break;
          }
        }
      }
    }
    enemy_shoot_timer = 0;
  }
}

void move_enemies(void) {
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (enemies[i].alive) {
      enemies[i].x += enemies[i].dx;
      enemies[i].y += enemies[i].dy;
      if (enemies[i].x <= 0 || enemies[i].x >= LCD_W - ENEMY_WIDTH)
        enemies[i].dx = -enemies[i].dx;
      if (enemies[i].y <= 0 || enemies[i].y >= LCD_H - ENEMY_HEIGHT)
        enemies[i].dy = -enemies[i].dy;
    }
  }
}

void boss_shoot(void) {
  boss_bullet_spawn_timer++;
  if (boss_bullet_spawn_timer > BOSS_BULLET_SPAWN_INTERVAL) {
    float angle_step = 2 * 3.1415926f / BOSS_BULLETS_PER_WAVE;
    for (int b = 0;
         b < BOSS_BULLETS_PER_WAVE && boss_bullet_count < MAX_BOSS_BULLETS;
         ++b) {
      for (int i = 0; i < MAX_BOSS_BULLETS; ++i) {
        if (!boss_bullets[i].alive) {
          float angle = b * angle_step;
          boss_bullets[i].x = BOSS_CENTER_X - BULLET_VISUAL_OFFSET;
          boss_bullets[i].y = BOSS_CENTER_Y - BULLET_VISUAL_OFFSET;
          boss_bullets[i].base_x = BOSS_CENTER_X;
          boss_bullets[i].base_y = BOSS_CENTER_Y;
          boss_bullets[i].t = 0;
          boss_bullets[i].angle = angle;
          boss_bullets[i].alive = 1;
          boss_bullet_count++;
          break;
        }
      }
    }
    boss_bullet_spawn_timer = 0;
  }
}

void move_bullet(void) {
  // Update boss bullets
  for (int i = 0; i < MAX_BOSS_BULLETS; ++i) {
    if (boss_bullets[i].alive) {
      boss_bullets[i].t += 1.0f;
      float spiral_growth_rate = 0.7f;
      float spiral_angular_speed = 0.12f;
      float current_radius = 10.0f + boss_bullets[i].t * spiral_growth_rate;
      float current_angle =
          boss_bullets[i].angle + boss_bullets[i].t * spiral_angular_speed;
      boss_bullets[i].x = boss_bullets[i].base_x +
                          current_radius * cosf(current_angle) -
                          BULLET_VISUAL_OFFSET;
      boss_bullets[i].y = boss_bullets[i].base_y +
                          current_radius * sinf(current_angle) -
                          BULLET_VISUAL_OFFSET;

      if (boss_bullets[i].x < -BULLET_STRAIGHT_DRAW_SIZE ||
          boss_bullets[i].x > LCD_W ||
          boss_bullets[i].y < -BULLET_STRAIGHT_DRAW_SIZE ||
          boss_bullets[i].y > LCD_H) {
        boss_bullets[i].alive = 0;
        boss_bullet_count--;
      }
    }
  }

  // Update regular enemy bullets
  for (int i = 0; i < MAX_REGULAR_ENEMY_BULLETS; ++i) {
    if (enemy_bullets[i].alive) {
      if (enemy_bullets[i].type == BULLET_TYPE_STRAIGHT) {
        enemy_bullets[i].x += enemy_bullets[i].dx;
        enemy_bullets[i].y += enemy_bullets[i].dy;
      } else if (enemy_bullets[i].type == BULLET_TYPE_SINE) {
        enemy_bullets[i].t += 1.0f;
        float freq = 0.15f;
        float amp = 8.0f; // Potentially different params for enemy sine
        float main_dir_x = cosf(enemy_bullets[i].angle);
        float main_dir_y = sinf(enemy_bullets[i].angle);
        float current_path_x =
            enemy_bullets[i].base_x +
            main_dir_x * enemy_bullets[i].path_speed * enemy_bullets[i].t;
        float current_path_y =
            enemy_bullets[i].base_y +
            main_dir_y * enemy_bullets[i].path_speed * enemy_bullets[i].t;
        float perp_x = -main_dir_y;
        float perp_y = main_dir_x;
        float offset = amp * sinf(enemy_bullets[i].t * freq);
        enemy_bullets[i].x = current_path_x + perp_x * offset;
        enemy_bullets[i].y = current_path_y + perp_y * offset;
      } else if (enemy_bullets[i].type == BULLET_TYPE_SPIRAL) {
        enemy_bullets[i].t += 1.0f;
        float spiral_growth_rate = 0.5f;
        float spiral_angular_speed = 0.15f; // Different params
        float current_radius = 5.0f + enemy_bullets[i].t * spiral_growth_rate;
        float current_angle =
            enemy_bullets[i].angle + enemy_bullets[i].t * spiral_angular_speed;
        enemy_bullets[i].x = enemy_bullets[i].base_x +
                             current_radius * cosf(current_angle) -
                             BULLET_VISUAL_OFFSET;
        enemy_bullets[i].y = enemy_bullets[i].base_y +
                             current_radius * sinf(current_angle) -
                             BULLET_VISUAL_OFFSET;
      }
      if (enemy_bullets[i].x < -BULLET_STRAIGHT_DRAW_SIZE ||
          enemy_bullets[i].x > LCD_W ||
          enemy_bullets[i].y < -BULLET_STRAIGHT_DRAW_SIZE ||
          enemy_bullets[i].y > LCD_H) {
        enemy_bullets[i].alive = 0;
        enemy_bullet_count--;
      }
    }
  }

  // Update player bullets
  for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
    if (player_bullets[i].alive) {
      player_bullets[i].x += player_bullets[i].dx;
      player_bullets[i].y += player_bullets[i].dy;

      int target_enemy_idx = player_bullets[i].target_idx;
      if (target_enemy_idx >= 0 && target_enemy_idx < MAX_ENEMIES &&
          enemies[target_enemy_idx].alive) {
        float p_bullet_cx = player_bullets[i].x + PLAYER_BULLET_CENTER_OFFSET;
        float p_bullet_cy = player_bullets[i].y + PLAYER_BULLET_CENTER_OFFSET;
        float enemy_cx = enemies[target_enemy_idx].x + ENEMY_CENTER_OFFSET;
        float enemy_cy = enemies[target_enemy_idx].y + ENEMY_CENTER_OFFSET;

        float dx_seek = enemy_cx - p_bullet_cx;
        float dy_seek = enemy_cy - p_bullet_cy;
        float len_seek = sqrtf(dx_seek * dx_seek + dy_seek * dy_seek);
        if (len_seek > 1.0f) {
          player_bullets[i].dx = PLAYER_BULLET_SPEED * dx_seek / len_seek;
          player_bullets[i].dy = PLAYER_BULLET_SPEED * dy_seek / len_seek;
        }

        if (fabsf(p_bullet_cx - enemy_cx) <
                COLLISION_THRESHOLD_PLAYER_BULLET_ENEMY &&
            fabsf(p_bullet_cy - enemy_cy) <
                COLLISION_THRESHOLD_PLAYER_BULLET_ENEMY) {
          enemies[target_enemy_idx].alive = 0;
          enemy_count--;
          player_bullets[i].alive = 0;
          player_bullet_count--;
        }
      }
      if (player_bullets[i].alive &&
          (player_bullets[i].x < -PLAYER_BULLET_DRAW_SIZE ||
           player_bullets[i].x > LCD_W ||
           player_bullets[i].y < -PLAYER_BULLET_DRAW_SIZE ||
           player_bullets[i].y > LCD_H)) {
        player_bullets[i].alive = 0;
        player_bullet_count--;
      }
    }
  }
}

void draw(void) {
  // Draw player
  LCD_Fill(player_x, player_y, player_x + player_size - 1,
           player_y + player_size - 1, RED);

  // Draw enemies
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (enemies[i].alive) {
      u16 enemy_color;
      switch (enemies[i].type) {
      case ENEMY_TYPE_NORMAL:
        enemy_color = GREEN;
        break;
      case ENEMY_TYPE_SINE_SHOOTER:
        enemy_color = CYAN;
        break;
      case ENEMY_TYPE_SPIRAL_SHOOTER:
        enemy_color = MAGENTA;
        break;
      default:
        enemy_color = WHITE;
      }
      LCD_Fill(enemies[i].x, enemies[i].y, enemies[i].x + ENEMY_WIDTH - 1,
               enemies[i].y + ENEMY_HEIGHT - 1, enemy_color);
    }
  }

  // Draw boss bullets
  for (int i = 0; i < MAX_BOSS_BULLETS; ++i) {
    if (boss_bullets[i].alive) {
      int bx_int = (int)boss_bullets[i].x;
      int by_int = (int)boss_bullets[i].y;
      int d_cx = bx_int + 2;
      int d_cy = by_int + 2;
      LCD_DrawLine(d_cx, d_cy - 3, d_cx + 3, d_cy, YELLOW);
      LCD_DrawLine(d_cx + 3, d_cy, d_cx, d_cy + 3, YELLOW);
      LCD_DrawLine(d_cx, d_cy + 3, d_cx - 3, d_cy, YELLOW);
      LCD_DrawLine(d_cx - 3, d_cy, d_cx, d_cy - 3, YELLOW);
    }
  }

  // Draw regular enemy bullets
  for (int i = 0; i < MAX_REGULAR_ENEMY_BULLETS; ++i) {
    if (enemy_bullets[i].alive) {
      int bx_int = (int)enemy_bullets[i].x;
      int by_int = (int)enemy_bullets[i].y;
      if (enemy_bullets[i].type == BULLET_TYPE_STRAIGHT) {
        LCD_Fill(bx_int, by_int, bx_int + BULLET_STRAIGHT_DRAW_SIZE - 1,
                 by_int + BULLET_STRAIGHT_DRAW_SIZE - 1, MAGENTA);
      } else if (enemy_bullets[i].type == BULLET_TYPE_SINE) {
        int tri_x[3] = {bx_int + 2, bx_int, bx_int + 4};
        int tri_y[3] = {by_int, by_int + 4, by_int + 4};
        LCD_DrawLine(tri_x[0], tri_y[0], tri_x[0], tri_y[1], CYAN);
        LCD_DrawLine(tri_x[1], tri_y[1], tri_x[2], tri_y[2], CYAN);
        // LCD_DrawLine(tri_x[2], tri_y[2], tri_x[0], tri_y[0], CYAN);
      } else if (enemy_bullets[i].type == BULLET_TYPE_SPIRAL) {
        int d_cx = bx_int + 2;
        int d_cy = by_int + 2;
        LCD_DrawLine(d_cx, d_cy - 3, d_cx + 3, d_cy, YELLOW);
        LCD_DrawLine(d_cx + 3, d_cy, d_cx, d_cy + 3, YELLOW);
        LCD_DrawLine(d_cx, d_cy + 3, d_cx - 3, d_cy, YELLOW);
        LCD_DrawLine(d_cx - 3, d_cy, d_cx, d_cy - 3, YELLOW);
      }
    }
  }

  // Draw player bullets
  for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
    if (player_bullets[i].alive) {
      LCD_Fill((int)player_bullets[i].x, (int)player_bullets[i].y,
               (int)player_bullets[i].x + PLAYER_BULLET_DRAW_SIZE - 1,
               (int)player_bullets[i].y + PLAYER_BULLET_DRAW_SIZE - 1, BLUE);
    }
  }

  // Draw boss site
  // LCD_Fill(BOSS_SITE_X, BOSS_SITE_Y, BOSS_SITE_X + BOSS_SITE_WIDTH - 1,
  //          BOSS_SITE_Y + BOSS_SITE_HEIGHT - 1, YELLOW);
}

void fps_entity(void) {
  uint32_t entity_count = boss_bullet_count * 4 + enemy_bullet_count +
                          player_bullet_count + many_bullets_count;
  // diamond is 4 of line bullet

  static uint64_t prev_frame_mtime_ns = 0;
  uint32_t fps;
  uint64_t tmp = get_timer_value(), current_frame_mtime_ns;
  do {
    current_frame_mtime_ns = get_timer_value();
  } while (tmp == current_frame_mtime_ns);

  uint64_t mtime_diff = current_frame_mtime_ns - prev_frame_mtime_ns;
  if (mtime_diff > 0) {
    uint32_t mtime_clk_freq = SystemCoreClock / 4;
    if (mtime_clk_freq > 0)
      fps = (uint32_t)(mtime_clk_freq / mtime_diff);
    else
      fps = 0; // SystemCoreClock not set or error
  } else
    fps = 99;

  prev_frame_mtime_ns = current_frame_mtime_ns;

  char entity_str[32];
  char fps_str[32];
  sprintf(entity_str, "Num: %03lu", (long unsigned int)entity_count);
  sprintf(fps_str, "FPS: %02lu", (long unsigned int)fps);
  LCD_ShowString(0, 0, (u8 *)entity_str, WHITE);
  LCD_ShowString(0, 15, (u8 *)fps_str, WHITE);
}

void erase_origin(void) {
  // Erase player
  if (prev_player_x != player_x || prev_player_y != player_y)
    LCD_Fill(prev_player_x, prev_player_y, prev_player_x + player_size - 1,
             prev_player_y + player_size - 1, BLACK);

  // Erase enemies
  for (int i = 0; i < MAX_ENEMIES; ++i) {
    if (enemies[i].prev_alive) {
      LCD_Fill(enemies[i].prev_x, enemies[i].prev_y,
               enemies[i].prev_x + ENEMY_WIDTH - 1,
               enemies[i].prev_y + ENEMY_HEIGHT - 1, BLACK);
    }
  }

  // Erase boss bullets
  for (int i = 0; i < MAX_BOSS_BULLETS; ++i) {
    if (boss_bullets[i].prev_alive) {
      int prev_bx_int = (int)boss_bullets[i].prev_x;
      int prev_by_int = (int)boss_bullets[i].prev_y;
      int d_cx = prev_bx_int + 2;
      int d_cy = prev_by_int + 2;
      LCD_DrawLine(d_cx, d_cy - 3, d_cx + 3, d_cy, BLACK);
      LCD_DrawLine(d_cx + 3, d_cy, d_cx, d_cy + 3, BLACK);
      LCD_DrawLine(d_cx, d_cy + 3, d_cx - 3, d_cy, BLACK);
      LCD_DrawLine(d_cx - 3, d_cy, d_cx, d_cy - 3, BLACK);
    }
  }

  // Erase enemy bullets
  for (int i = 0; i < MAX_REGULAR_ENEMY_BULLETS; ++i) {
    if (enemy_bullets[i].prev_alive) {
      int prev_bx_int = (int)enemy_bullets[i].prev_x;
      int prev_by_int = (int)enemy_bullets[i].prev_y;
      if (enemy_bullets[i].type == BULLET_TYPE_STRAIGHT) {
        LCD_Fill(prev_bx_int, prev_by_int,
                 prev_bx_int + BULLET_STRAIGHT_DRAW_SIZE - 1,
                 prev_by_int + BULLET_STRAIGHT_DRAW_SIZE - 1, BLACK);
      } else if (enemy_bullets[i].type == BULLET_TYPE_SINE) {
        int tri_x[3] = {prev_bx_int + 2, prev_bx_int, prev_bx_int + 4};
        int tri_y[3] = {prev_by_int, prev_by_int + 4, prev_by_int + 4};
        LCD_DrawLine(tri_x[0], tri_y[0], tri_x[0], tri_y[1], BLACK);
        LCD_DrawLine(tri_x[1], tri_y[1], tri_x[2], tri_y[2], BLACK);
        // LCD_DrawLine(tri_x[2], tri_y[2], tri_x[0], tri_y[0], BLACK);
      } else if (enemy_bullets[i].type == BULLET_TYPE_SPIRAL) {
        int d_cx = prev_bx_int + 2;
        int d_cy = prev_by_int + 2;
        LCD_DrawLine(d_cx, d_cy - 3, d_cx + 3, d_cy, BLACK);
        LCD_DrawLine(d_cx + 3, d_cy, d_cx, d_cy + 3, BLACK);
        LCD_DrawLine(d_cx, d_cy + 3, d_cx - 3, d_cy, BLACK);
        LCD_DrawLine(d_cx - 3, d_cy, d_cx, d_cy - 3, BLACK);
      }
    }
  }

  // Erase player bullets
  for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
    if (player_bullets[i].prev_alive) {
      LCD_Fill((int)player_bullets[i].prev_x, (int)player_bullets[i].prev_y,
               (int)player_bullets[i].prev_x + PLAYER_BULLET_DRAW_SIZE - 1,
               (int)player_bullets[i].prev_y + PLAYER_BULLET_DRAW_SIZE - 1,
               BLACK);
    }
  }
}

void store_state(void) {
  prev_player_x = player_x;
  prev_player_y = player_y;

  for (int i = 0; i < MAX_ENEMIES; ++i) {
    enemies[i].prev_x = enemies[i].x;
    enemies[i].prev_y = enemies[i].y;
    enemies[i].prev_alive = enemies[i].alive;
  }
  for (int i = 0; i < MAX_BOSS_BULLETS; ++i) {
    boss_bullets[i].prev_x = boss_bullets[i].x;
    boss_bullets[i].prev_y = boss_bullets[i].y;
    boss_bullets[i].prev_alive = boss_bullets[i].alive;
  }
  for (int i = 0; i < MAX_REGULAR_ENEMY_BULLETS; ++i) {
    enemy_bullets[i].prev_x = enemy_bullets[i].x;
    enemy_bullets[i].prev_y = enemy_bullets[i].y;
    enemy_bullets[i].prev_alive = enemy_bullets[i].alive;
  }
  for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
    player_bullets[i].prev_x = player_bullets[i].x;
    player_bullets[i].prev_y = player_bullets[i].y;
    player_bullets[i].prev_alive = player_bullets[i].alive;
  }
}

void draw_many_bullets(void);
void update_many_bullets(void);
void spawn_many_bullets(void);
void erase_many_bullets(void);
void store_many_bullets(void);

extern int choice;

int main(void) {
  IO_init();
  LCD_Clear(BLACK);
  int default_choice = 0;
  int mode = start(default_choice);

  // Player position
  player_x = 30, player_y = 30;
  player_size = 6 + mode - choice;
  player_center_offset = player_size / 2;

  // Initial screen clear
  LCD_Clear(BLACK);

  // Initial draw of static elements
  // LCD_Fill(BOSS_SITE_X, BOSS_SITE_Y, BOSS_SITE_X + BOSS_SITE_WIDTH - 1,
  //          BOSS_SITE_Y + BOSS_SITE_HEIGHT - 1, YELLOW);

  // Initial player draw and prev state setup
  LCD_Fill(player_x, player_y, player_x + player_size - 1,
           player_y + player_size - 1, RED);

  prev_player_x = player_x;
  prev_player_y = player_y;

  int boom = 80;

  while (1) {
    // --- GAME LOGIC PHASE ---
    static uint64_t last_action_time_joy_left = 0;
    static uint64_t last_action_time_joy_right = 0;
    static uint64_t last_action_time_joy_up = 0;
    static uint64_t last_action_time_joy_down = 0;
    uint64_t current_time = get_timer_value();

    // Handle player movement
    if (Get_Button(JOY_LEFT) && player_x > 0 &&
        (current_time - last_action_time_joy_left) >
            BUTTON_ACTION_COOLDOWN_TICKS) {
      player_x -= PLAYER_SPEED;
      last_action_time_joy_left = current_time;
    }
    if (Get_Button(JOY_RIGHT) && player_x < LCD_W - player_size &&
        (current_time - last_action_time_joy_right) >
            BUTTON_ACTION_COOLDOWN_TICKS) {
      player_x += PLAYER_SPEED;
      last_action_time_joy_right = current_time;
    }
    if (Get_Button(JOY_UP) && player_y > 0 &&
        (current_time - last_action_time_joy_up) >
            BUTTON_ACTION_COOLDOWN_TICKS) {
      player_y -= PLAYER_SPEED;
      last_action_time_joy_up = current_time;
    }
    if (Get_Button(JOY_DOWN) && player_y < LCD_H - player_size &&
        (current_time - last_action_time_joy_down) >
            BUTTON_ACTION_COOLDOWN_TICKS) {
      player_y += PLAYER_SPEED;
      last_action_time_joy_down = current_time;
    }

    // Player shoot
    if (Get_Button(BUTTON_1)) {
      player_shoot();
    }

    if (boom > 0) {
      boom--;
      spawn_many_bullets(); // for 256
    }
    update_many_bullets();

    if (many_bullets_count == 0) {
      spawn_enemies();
      enemies_shoot();
      move_enemies();
      boss_shoot();
    }

    move_bullet();

    // --- DRAW PHASE ---
    fps_entity();
    draw();
    draw_many_bullets();

    // --- ERASE PHASE ---
    erase_origin();
    erase_many_bullets();

    // --- STORE STATE PHASE ---
    store_state();
    store_many_bullets();

    delay_1ms(5);
  }
}

void draw_many_bullets(void) {
  // Draw bullets at new
  for (int i = 0; i < MANY_BULLET; ++i) {
    if (bullets[i].alive) {
      int bx_int = (int)bullets[i].x;
      int by_int = (int)bullets[i].y;
      LCD_DrawPoint(bx_int, by_int, WHITE);
    }
    if (bullets[i].prev_alive) {
      int prev_bx_int = (int)bullets[i].prev_x;
      int prev_by_int = (int)bullets[i].prev_y;
      LCD_DrawPoint(prev_bx_int, prev_by_int, BLACK);
    }
  }
}

void update_many_bullets(void) {

  // Update bullets
  for (int i = 0; i < MANY_BULLET; ++i) {
    if (bullets[i].alive) {
      bullets[i].x += bullets[i].dx;

      if (bullets[i].x < -BULLET_STRAIGHT_DRAW_SIZE || bullets[i].x > LCD_W ||
          bullets[i].y < -BULLET_STRAIGHT_DRAW_SIZE || bullets[i].y > LCD_H) {
        bullets[i].alive = 0;
        many_bullets_count--;
      }
    }
  }
}

void spawn_many_bullets(void) {
  // Spawn new bullets
  int timer = 0;
  for (int i = 0; i < MANY_BULLET && timer < 5; ++i) {
    if (!bullets[i].alive) {
      bullets[i].x = 1.0f;
      bullets[i].y = (float)(i % (LCD_H - 4) + 2);
      bullets[i].dx = 2.0f;
      bullets[i].dy = 0;
      bullets[i].alive = 1;

      timer++;
      many_bullets_count++;
    }
  }
}

void store_many_bullets(void) {
  for (int i = 0; i < MANY_BULLET; ++i) {
    bullets[i].prev_x = bullets[i].x;
    bullets[i].prev_y = bullets[i].y;
    bullets[i].prev_alive = bullets[i].alive;
  }
}

void erase_many_bullets(void) { return; }
