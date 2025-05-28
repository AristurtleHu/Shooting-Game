# Project 4: STG (Shooting Game)

## Overview
Implement a STG using C language and RISC-V assembly for the Longan Nano development board.

## Reference Implementation
**proj4-25s-sample.bin**

## Implementation Steps

### Basic
This is the basic part and focuses on knowledge learnt in class.

#### 1.1 RISCV Code
Implement scenario selection screen (with at least 3 choices) as a RISC-V function.

Did not call any C function other than `LCD_ShowString` and input-related functions.

#### 1.2 Basic Gameplay Experience
1. Some pixels represent the player on the screen and the player can move in response to keyboard input.

2. Player can shoot.

3. There are pixels representing enemies on the screen and the enemy can move.

4. Enemies can shoot. Player doesn't die when being hit by the enemy's bullets, which means ignoring the hitbox of player.

#### 1.3 Basic Physical Features in Hardware

Implement an algorithm that reads button input, and filters out noises or other instabilities. Specifically, if the user pressed key `K` at time `t` and this causes `K` to be triggered, then `K` cannot be triggered again before `t + \Delta t`, while other keys can.

#### 1.4 Memory Management

Implement memory correctly without crush or stop responding on rigorous stress testing.

#### 1.5 Basic Performance Requirement

Screen does not blink, and FPS is above 25.

The FPS counter shows the averaged result from 0.12-0.15 seconds consecutively to improve readability.

### Improvement

#### 2.1 Performance

Keep minimal FPS during normal gameplay above 30, and support at least 512 bullets (including the bullets shot by the player and the enemy) on the screen simultaneously. Show an entity counter on screen to know how many bullets you support.

The FPS counter and entity counter show the averaged result from 0.12-0.15 seconds consecutively to improve readability.

*Tip:* Customized the LCD library for maximum efficiency.

#### 2.2 Homing Bullets

1. Player shoot bullets that seek towards the enemy.
2. Longan Nano does not support the F-extension instructions of RISC-V in its hardware, but the compiler automatically inserts software floating point number library into the code.

#### 2.3 Extended Contents
1. Implement at least 3 types of bullets that is different in shape.

2. Implement at least 3 types of bullets that is different in trajectory. Homing bullet does not count here.

3. Implement at least 3 types of enemy with different shooting pattern.

Integrate them into one complete scenario with playable content of at least 1 minute.

*Note:* Shape or trajectory A and B is different from each other, if and only if there exists no way to turn A into B via arbitrary combination of scaling, translation, and rotation. We do not consider color difference in 2.3.