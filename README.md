CS/EE120B Project Proposal: Cyclic Puzzle
=========================================
#### Introduction
Cyclic Puzzle is a two-dimensional puzzle game that tests the player's reaction time and hand-eye coordination. The first line of the 16x2 LED Display will generate a randomly populated pattern (puzzle) that the player will need to match. The second line of the display will utilize a linearly moving indicator that the player must use to match the pattern. This indicator cannot be controlled by the player and moves continuously until the player finishes the puzzle, resets the game, or runs out of time (90 seconds). The indicator's speed and the number of populated locations will change according to the difficulty the player sets. The goal is to match the puzzle on the first line in the shortest amount of time.

![Cyclic Puzzle Sim](http://shawnjzlee.me/img/EE120B_Project_Sim.gif)

#### Components

* **PortA 0-7** | 16x2 LCD screen - used to display the game. 

* **PortB 0** | Button - used to reset the game. 

* **PortD 0-7** | Keypad - used for user input (level selection, menu navigation, gameplay).

* **PortC 0-7** | Two (2) 7-Segment Displays - used to display time in seconds.

#### Complexities / Build Upon

1. Using the 16x2 LCD screen to display the game, the menu, and high-scores.

2. Using EEPROM to save the high score of players.

3. Using two 7-segment displays to output the number of seconds on the timer.

4. Random generation of different lengths and the pattern locations of the puzzle.

#### How To Play

![Cyclic Puzzle Feature](http://shawnjzlee.me/img/EE120B_Feature.gif)

* When the indicator reaches a hollow cell and the keypad is pressed, the cell will be filled.

* However, when the indicator reaches a filled cell and the keypad is pressed, the cell will be hollow again.

* All hollow cells in the puzzle must be filled to win the game.

#### Game Levels

| Level | Hollow Cells | Total Cells  |
| ----- |:------------:| ------------:|
| 1     | up to 5      | 6            |
| 2     | up to 6      | 7            |
| 3     | up to 7      | 8            |
| 4     | up to 8      | 9            |
| 5     | up to 9      | 10           |
| 6     | up to 10     | 11           |
| 7     | up to 11     | 12           |
| 8     | up to 13     | 14           |
| 9     | up to 15     | 16           |