/*
 * digit.h
 *
 *  Created on: 22.08.2021
 *      Author: Uwe
 */

#ifndef DIGIT_H_
#define DIGIT_H_

// settings
#define PRE_MOVE 			true		// true to start move earlier to reach the end position just in time
#define DEBUG 				true		// true for additional serial debug messages

// Motor parameters
#define STEPS_PER_ROTATION 	4096 		// steps of a single rotation of motor
#define MOTOR_DELAY 		1200
#define KILL_BACKLASH 		15
#define INITIAL_POS 		13
#define POSITION 			13 			// each wheel has 9 positions
#define HOUR12 				false

#define DIGIT 4
typedef struct
{
	int v[DIGIT];
} Digit;


#endif /* DIGIT_H_ */
