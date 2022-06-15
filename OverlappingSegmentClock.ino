// Hollow Clock improved by SnowHead, Mart 2021
//
// Thanks to:
// shiura for stepper motor routines
// SnowHead for Internet-Time

#include "digit.h"
#include "WTAClient.h"

//=== CLOCK ===
WTAClient wtaClient;
time_t locEpoch = 0, netEpoch = 0;
extern unsigned long askFrequency;

//////////////////////////////
// motor control section
//////////////////////////////

// ports used to control the stepper motor
// if your motor rotate to the opposite direction,
// change the order as {15, 13, 12, 14};
// ESP	 Stepper
// D5 -> IN1
// D6 -> IN2
// D7 -> IN3
// D8 -> IN4
int port[4] =
{ 14, 12, 13, 15 };

// digit definition. 10 for blank
int table[10] =
{ 0, 10, 6, 2, 12, 7, 5, 11, 4, 3 };

static long currentPos;
static long currentStep;
static Digit current =
{ 0 };

#if PRE_MOVE
int j = 0;
static unsigned long time_needed = 0;	// sum of time for next move
Digit timedig =
{ 0 };
Digit tcurrent =
{ 0, 0, 0, 0 };		// store current digit position
bool real_move = true;					// move motor real
#endif

// sequence of stepper motor control
int seq[8][4] =
{
{ LOW, HIGH, HIGH, LOW },
{ LOW, LOW, HIGH, LOW },
{ LOW, LOW, HIGH, HIGH },
{ LOW, LOW, LOW, HIGH },
{ HIGH, LOW, LOW, HIGH },
{ HIGH, LOW, LOW, LOW },
{ HIGH, HIGH, LOW, LOW },
{ LOW, HIGH, LOW, LOW } };

// stepper motor rotation
void rotate(int step)
{
	int count = 0;
	static int phase = 0;
	int i, j;
	int delta = (step > 0) ? 1 : 7;

#if DEBUG
#if PRE_MOVE
	if (real_move)
#endif
	{
		Serial.print("rotating steps: ");
		Serial.println(step);
	}
#endif
	step = labs(step);
#if PRE_MOVE
	if (!real_move)
	{
		for (j = 0; j < step; j++)
		{
			count++;
			time_needed += MOTOR_DELAY;
			if ((step - j) < 100)
				time_needed += MOTOR_DELAY; // deacceleration
			if ((step - j) < 200)
				time_needed += MOTOR_DELAY; // deacceleration
			if ((step - j) < 400)
				time_needed += MOTOR_DELAY; // deacceleration
			if (count < 100)
				time_needed += MOTOR_DELAY; // acceleration
			if (count < 50)
				time_needed += MOTOR_DELAY; // acceleration
			if (count < 20)
				time_needed += MOTOR_DELAY; // acceleration
			if (count < 10)
				time_needed += MOTOR_DELAY; // acceleration
		}
		return;
	}
#endif
	for (j = 0; j < step; j++)
	{
		phase = (phase + delta) % 8;
		for (i = 0; i < 4; i++)
		{
			digitalWrite(port[i], seq[phase][i]);
		}
		count++;
		delayMicroseconds(MOTOR_DELAY);
		if ((step - j) < 100)
			delayMicroseconds(MOTOR_DELAY); // deacceleration
		if ((step - j) < 200)
			delayMicroseconds(MOTOR_DELAY); // deacceleration
		if ((step - j) < 400)
			delayMicroseconds(MOTOR_DELAY); // deacceleration
		if (count < 100)
			delayMicroseconds(MOTOR_DELAY); // acceleration
		if (count < 50)
			delayMicroseconds(MOTOR_DELAY); // acceleration
		if (count < 20)
			delayMicroseconds(MOTOR_DELAY); // acceleration
		if (count < 10)
			delayMicroseconds(MOTOR_DELAY); // acceleration
		ESP.wdtFeed();
	}
	// power cut
	for (i = 0; i < 4; i++)
	{
		digitalWrite(port[i], LOW);
	}
}

//////////////////////////////
// rorary clock control section
//////////////////////////////

void printDigit(Digit d);
Digit rotUp(Digit rcurrent, int digit, int num);
Digit rotDown(Digit rcurrent, int digit, int num);
Digit rotDigit(Digit rcurrent, int digit, int num);
Digit setDigit(Digit scurrent, int digit, int num);
int setNumber(Digit n);

// avoid error accumulation of fractional part of 4096 / POSITION
void rotStep(int s)
{
	currentPos += s;
	long diff = currentPos * STEPS_PER_ROTATION / POSITION - currentStep;
	if (diff < 0)
		diff -= KILL_BACKLASH;
	else
		diff += KILL_BACKLASH;
	rotate(diff);
	currentStep += diff;
}

void printDigit(Digit d)
{
	char s[32];

	sprintf(s, "%1X%1X%1X%1X", d.v[0], d.v[1], d.v[2], d.v[3]);
	Serial.println(s);
}

//increase specified digit
Digit rotUp(Digit rcurrent, int digit, int num)
{
	int freeplay = 0;
	int i;

	for (i = digit; i < DIGIT - 1; i++)
	{
		int id = rcurrent.v[i];
		int nd = rcurrent.v[i + 1];
		if (id <= nd)
			id += POSITION;
		freeplay += id - nd - 1;
	}
	freeplay += num;
	rotStep(-1 * freeplay);
	rcurrent.v[digit] = (rcurrent.v[digit] + num) % POSITION;
	for (i = digit + 1; i < DIGIT; i++)
	{
		rcurrent.v[i] = (rcurrent.v[i - 1] + (POSITION - 1)) % POSITION;
	}

#if DEBUG
#if PRE_MOVE
	if (real_move)
#endif
	{
		Serial.print("up end : ");
		printDigit(rcurrent);
	}
#endif
	return rcurrent;
}

// decrease specified digit
Digit rotDown(Digit rcurrent, int digit, int num)
{
	int freeplay = 0;
	int i;

	for (i = digit; i < DIGIT - 1; i++)
	{
		int id = rcurrent.v[i];
		int nd = rcurrent.v[i + 1];
		if (id > nd)
			nd += POSITION;
		freeplay += nd - id;
	}
	freeplay += num;
	rotStep(1 * freeplay);
	rcurrent.v[digit] = (rcurrent.v[digit] - num + POSITION) % POSITION;
	for (i = digit + 1; i < DIGIT; i++)
	{
		rcurrent.v[i] = rcurrent.v[i - 1];
	}

#if DEBUG
#if PRE_MOVE
	if (real_move)
#endif
	{
		Serial.print("down end : ");
		printDigit(rcurrent);
	}
#endif
	return rcurrent;
}

// decrease or increase specified digit
Digit rotDigit(Digit rcurrent, int digit, int num)
{
	if (num > 0)
	{
		return rotUp(rcurrent, digit, num);
	}
	else if (num < 0)
	{
		return rotDown(rcurrent, digit, -num);
	}
	else
		return rcurrent;
}

// set single digit to the specified number
Digit setDigit(Digit scurrent, int digit, int num)
{
	int pd, cd = scurrent.v[digit];
	if (digit == 0)
	{ // most significant digit
		pd = 0;
	}
	else
	{
		pd = scurrent.v[digit - 1];
	}
	if (cd == num)
		return scurrent;

	// check if increasing rotation is possible
	int n2 = num;
	if (n2 < cd)
		n2 += POSITION;
	if (pd < cd)
		pd += POSITION;
	if (pd <= cd || pd > n2)
	{
		return rotDigit(scurrent, digit, n2 - cd);
	}
	// if not, do decrease rotation
	if (num > cd)
		cd += POSITION;
	return rotDigit(scurrent, digit, num - cd);
}

int setNumber(Digit n)
{
	int i;

	for (i = 0; i < DIGIT; i++)
	{
		if (current.v[i] != n.v[i])
		{
			break;
		}
	}
	// rotate most significant wheel only to follow current time ASAP
	if (i < DIGIT)
	{
		current = setDigit(current, i, n.v[i]);
	}
	if (i >= DIGIT - 1)
	{
		return true; // finished
	}
	else
	{
		return false;
	}
}

//////////////////////////////
// setup and main loop
//////////////////////////////

void setup()
{
	*((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
	Serial.begin(115200);
	Serial.println("start");

	pinMode(port[0], OUTPUT);
	pinMode(port[1], OUTPUT);
	pinMode(port[2], OUTPUT);
	pinMode(port[3], OUTPUT);

	wtaClient.Setup();
	askFrequency = 50;

	rotate(STEPS_PER_ROTATION * DIGIT); // reset all digits using physical end stop
	rotate(-INITIAL_POS * DIGIT); // release pushing force to align all digits better
}

void loop()
{
	static int prevmin = -1;
	struct tm *tmtime;
	Digit n;

	askFrequency = 60 * 60 * 1000;
	netEpoch = wtaClient.GetCurrentTime();

#if PRE_MOVE
	long tcurrentPos;
	long tcurrentStep;
	time_t tEpoch;

	netEpoch += ((time_needed + 500) / 1000);
#endif

	tmtime = localtime(&netEpoch);

#if HOUR12
	tmtime->tm_hour %= 12;
	if (tmtime->tm_hour == 0)
	{
		tmtime->tm_hour = 12;
	}
#endif

	if (tmtime->tm_min != prevmin)
	{
		n.v[0] = table[tmtime->tm_hour / 10];
		n.v[1] = table[tmtime->tm_hour % 10];
		n.v[2] = table[tmtime->tm_min / 10];
		n.v[3] = table[tmtime->tm_min % 10];
#if DEBUG
		{
			char tstr[64];

			sprintf(tstr, "Move to %02d:%02d", tmtime->tm_hour, tmtime->tm_min);
			Serial.println(tstr);
		}
#endif
		while (!setNumber(n))
			;
		prevmin = tmtime->tm_min;
#if PRE_MOVE
		real_move = false;
		time_needed = 0;
		j = 0;
		tcurrent = current;
		tcurrentPos = currentPos;
		tcurrentStep = currentStep;
		tEpoch = netEpoch + 61;
		tmtime = localtime(&tEpoch);
		timedig.v[j++] = table[tmtime->tm_hour / 10];
		timedig.v[j++] = table[tmtime->tm_hour % 10];
		timedig.v[j++] = table[tmtime->tm_min / 10];
		timedig.v[j] = table[tmtime->tm_min % 10];
		while (!setNumber(timedig))
			;
		time_needed /= 1000UL;
#if DEBUG
		Serial.print("calculated time for next move [ms]: ");
		Serial.println(time_needed);
#endif
		current = tcurrent;
		currentPos = tcurrentPos;
		currentStep = tcurrentStep;
		real_move = true;
#endif
	}
	delay(100);
}
