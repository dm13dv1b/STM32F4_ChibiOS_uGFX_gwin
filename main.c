

/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
                 2011 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ch.h"
#include "hal.h"
#include "gfx.h"

//ADC CallBack
static void adccb(ADCDriver *adcp, adcsample_t *buffer, size_t n);

/* Total number of channels to be sampled by a single ADC operation.*/
#define ADC_GRP1_NUM_CHANNELS   2

/* Depth of the conversion buffer, channels are sampled four times each.*/
#define ADC_GRP1_BUF_DEPTH      4


/*
 * ADC samples buffer.
 */
static adcsample_t samples[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 4 samples of 2 channels, SW triggered.
 * Channels:    IN11   (48 cycles sample time)
 *              Sensor (192 cycles sample time)
 */
static const ADCConversionGroup adcgrpcfg = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  adccb,
  NULL,
  /* HW dependent part.*/
  0,
  ADC_CR2_SWSTART,
  ADC_SMPR1_SMP_AN11(ADC_SAMPLE_56) | ADC_SMPR1_SMP_SENSOR(ADC_SAMPLE_144),
  0,
  ADC_SQR1_NUM_CH(ADC_GRP1_NUM_CHANNELS),
  0,
  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN11) | ADC_SQR3_SQ1_N(ADC_CHANNEL_SENSOR)
};

// general constants
static GConsoleObject			gc;
static GListener				gl;
static GHandle					ghc;
static GHandle					ghADC1, ghADC2, ghRPM, ghSpeed, ghBrightness;
static GHandle					ghStatus1, ghStatus2, ghADC;
static GHandle					ghStartADC, ghStopADC;
static GEventMouse				*pem;
static GEvent*					pe;
static coord_t 					width, height;
static coord_t					bWidth, bHeight;
static coord_t					swidth, sheight;
static font_t					font;

/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */

static WORKING_AREA(waThread1, 256);
static msg_t Thread1(void *arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
    palSetPad(GPIOD, GPIOD_LED3);       /* Orange.  */
    gwinSetText(ghStatus1, "Running", TRUE);

    chThdSleepMilliseconds(500);
    palClearPad(GPIOD, GPIOD_LED3);     /* Orange.  */
    gwinSetText(ghStatus1, "", TRUE);
    chThdSleepMilliseconds(500);
  }
}

/*
 * ADC end conversion callback.
 * The PWM channels are reprogrammed using the latest ADC samples.
 * The latest samples are transmitted into a single SPI transaction.
 */
void adccb(ADCDriver *adcp, adcsample_t *buffer, size_t n) {

  (void) buffer; (void) n;
  /* Note, only in the ADC_COMPLETE state because the ADC driver fires an
     intermediate callback when the buffer is half full.*/
  gwinSetText(ghADC, "ADC on", TRUE);
  if (adcp->state == ADC_COMPLETE) {
    adcsample_t avg_ch1, avg_ch2;
    gwinSetText(ghADC, "ADC complete", TRUE);
    /* Calculates the average values from the ADC samples.*/
    avg_ch1 = (samples[0] + samples[2] + samples[4] + samples[6]) / 4;
    avg_ch2 = (samples[1] + samples[3] + samples[5] + samples[7]) / 4;
  }
}

static void createWidgets(void)
{
	bHeight = gdispGetFontMetric(font, fontHeight)+2;

	GWidgetInit	wi;
	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;

	wi.g.y = 0;
	wi.g.x = 200;
	wi.g.width = 70;
	wi.g.height = bHeight;
	wi.text = "ADC Status";

	ghADC = gwinLabelCreate(NULL, &wi);

	//
	wi.g.y = 0;
	wi.g.x = 0;
	wi.g.width = 100;
	wi.g.height = bHeight;
	wi.text = "ADC1:";

	ghADC1 = gwinLabelCreate(NULL, &wi);

	//

	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;

	wi.g.y = bHeight;
	wi.g.x = 0;
	wi.g.width = 100;
	wi.g.height = bHeight;
	wi.text = "ADC2:";

	ghADC2 = gwinLabelCreate(NULL, &wi);

	//

	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;

	wi.g.y = bHeight*2;
	wi.g.x = 0;
	wi.g.width = 100;
	wi.g.height = bHeight;
	wi.text = "RPM:";

	ghRPM = gwinLabelCreate(NULL, &wi);

	//

	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;

	wi.g.y = bHeight*3;
	wi.g.x = 0;
	wi.g.width = 100;
	wi.g.height = bHeight;
	wi.text = "Speed:";

	ghSpeed = gwinLabelCreate(NULL, &wi);

	//

	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;

	wi.g.y = sheight-bHeight;
	wi.g.x = 0;
	wi.g.width = 160;
	wi.g.height = bHeight;
	wi.text = "Status1";

	ghStatus1 = gwinLabelCreate(NULL, &wi);

	//

	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;

	wi.g.y = sheight-bHeight;
	wi.g.x = 160;
	wi.g.width = 160;
	wi.g.height = bHeight;
	wi.text = "Status2";

	ghStatus2 = gwinLabelCreate(NULL, &wi);

	//
	wi.g.y = sheight-(bHeight*2);
	wi.g.x = 0;
	wi.g.width = swidth;
	wi.g.height = bHeight;
	wi.text = "Brightness";

	ghBrightness = gwinSliderCreate(NULL, &wi);
	gwinSliderSetRange(ghBrightness, 0, 100);
	gwinSliderSetPosition(ghBrightness, 50);

	//

	bWidth = gdispGetStringWidth("Start ADC", font);

	wi.g.y = sheight-(bHeight*4);
	wi.g.x = 10;
	wi.g.width = bWidth+8;
	wi.g.height = bHeight+4;
	wi.text = "Start ADC";

	ghStartADC = gwinButtonCreate(NULL, &wi);

	//
	bWidth = gdispGetStringWidth("Stop ADC", font);

	wi.g.y = sheight-(bHeight*4);
	wi.g.x = 10+bWidth+8+10;
	wi.g.width = bWidth+8;
	wi.g.height = bHeight+4;
	wi.text = "Stop ADC";

	ghStopADC = gwinButtonCreate(NULL, &wi);
}

static void bootScreen(void)
{
	  /* enyém */
	  width = gdispGetWidth();
	  height = gdispGetHeight();

	  swidth = gdispGetWidth();
	  sheight = gdispGetHeight();

		// Create our title
	  gwinAttachMouse(0);

		font = gdispOpenFont("UI2");
		gwinSetDefaultFont(font);
		gwinSetDefaultBgColor(Black);
		gwinSetDefaultColor(White);
		bHeight = gdispGetFontMetric(font, fontHeight)+2;
		gdispFillStringBox(0, 0, swidth, bHeight, "Boot", font, Red, White, justifyLeft);

		// Create our main display window
		{
			GWindowInit				wi;

			gwinClearInit(&wi);
			wi.show = TRUE;
			wi.x = 0;
			wi.y = bHeight;
			wi.width = swidth;
			wi.height = height-bHeight;
			ghc = gwinConsoleCreate(&gc, &wi);
		}

		//gwinClear(ghc);
		pem = (GEventMouse *)&gl.event;
		while (pem->meta & GMETA_MOUSE_UP);
		gwinPrintf(ghc, "Boot finished.\n");
}

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  chThdSleepMilliseconds(100);

  adcStart(&ADCD1, NULL);
  adcSTM32EnableTSVREFE();
  palSetPadMode(GPIOC, 1, PAL_MODE_INPUT_ANALOG);

  /* initialize and clear the display */
  gfxInit();
  bootScreen();
  gwinSetColor(ghc, White);
  gwinPrintf(ghc, "Calibartion finished.\n");
  gwinPrintf(ghc, "Starting USART2.\n");

  /*
   * Activates the serial driver 1 using the driver default configuration.
   * PA2(TX) and PA3(RX) are routed to USART2.
   */
  sdStart(&SD2, NULL);
  palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));

  /*
   * If the user button is pressed after the reset then the test suite is
   * executed immediately before activating the various device drivers in
   * order to not alter the benchmark scores.
   */
  //if (palReadPad(GPIOA, GPIOA_BUTTON))
  //  TestThread(&SD2);

  /*
   * Creates the example thread.
   */
  gwinPrintf(ghc, "Create waThread1\n");
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state, when the button is
   * pressed the test procedure is launched with output on the serial
   * driver 2.
   */

  /*
   * Initializes the ADC driver 1 and enable the thermal sensor.
   * The pin PC1 on the port GPIOC is programmed as analog input.
   */

  gwinPrintf(ghc, "ADC start\n");

  chThdSleepMilliseconds(5000);

  gwinSetDefaultFont(gdispOpenFont("UI2"));
  gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);
  gdispClear(White);
  createWidgets();
  geventListenerInit(&gl);
  gwinAttachListener(&gl);

  while (TRUE)
  {
	  //get an event
	  pe = geventEventWait(&gl, TIME_INFINITE);

	  switch(pe->type) {
	  	  case 	GEVENT_GWIN_SLIDER:
	  		  //printf("Slider s% = %d\n", gwinGetText(((GEventGWinSlider *)pe)->slider),
	  		  //	  ((GEventGWinSlider*)pe)->position);
	  		  	   break;
	  	  case GEVENT_GWIN_BUTTON:
	  		   	   if (((GEventGWinButton*)pe)->button == ghStartADC)
	  		   	   	   {
	  		   		   	   gwinSetText(ghStatus2, "ADC start", TRUE);
	  		   		   	   //chSysLockFromIsr();
	  		   		   	   adcStartConversionI(&ADCD1, &adcgrpcfg, samples, ADC_GRP1_BUF_DEPTH);
	  		   		   	   //chSysUnlockFromIsr();
	  		   	   	   };
	  		   	   if (((GEventGWinButton*)pe)->button == ghStopADC)
	  		   	   	   {
	  		   		  	   gwinSetText(ghStatus2, "ADC stop", TRUE);
	  		   	   	   };
	  		   	   break;
				default:
				   break ;
	  }
		//chThdSleepMilliseconds(500);
  }
}

