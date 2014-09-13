// PaceSplits (c) 2014 Keith Blom - All rights reserved
// v1.0 - 

#define APP_VERSION "Ver 1.0"

// Standard includes
#include "pebble.h"

// Forward declarations.

// *** Windows
static Window *menuWindow;
static Window *psWindow; 
static Window *setWindow; 

// *** Layers
// Main menu layer
#define NBR_MENU_ITEMS 5
static SimpleMenuLayer *menuLayer;
static SimpleMenuItem menuItems[NBR_MENU_ITEMS];
static SimpleMenuSection menuSection[1];

// Pace splits layers
static TextLayer *psChronoLayer;
static TextLayer *psSplitsLayer;
static BitmapLayer *ssIconLayer;

// Set Common Layers and associated data
#define BUTTON_REPEAT_INTERVAL 150
int setFieldNbr;
static TextLayer *setTitleLayer;
static TextLayer *setDataLayer;
static BitmapLayer *nextIconLayer;

// Race Distance Layers and associated data
#define rdIntegerGRect GRect(38, 44, 34, 28)
static TextLayer *rdIntegerLayer;
static InverterLayer *rdIntegerInverterLayer;
#define rdIntegerDecimalPointGRect GRect(72, 44, 18, 28)
static TextLayer *rdDecimalPointLayer;
#define rdTenthsGRect GRect(90, 44, 18, 28)
static TextLayer *rdTenthsLayer;
static InverterLayer *rdTenthsInverterLayer;

// Finish Time Layers and associated data
#define ftHoursGRect GRect(28, 44, 34, 28)
static TextLayer *ftHoursLayer;
static InverterLayer *ftHoursInverterLayer;
#define ftColonGRect GRect(62, 44, 18, 28)
static TextLayer *ftColonLayer;
#define ftMinsGRect GRect(80, 44, 34, 28)
static TextLayer *ftMinsLayer;
static InverterLayer *ftMinsInverterLayer;

// Split Distance Layers and associated data
#define splitDistGRect GRect(46, 44, 52, 28)
static TextLayer *splitDistLayer;
static InverterLayer *splitDistInverterLayer;

// *** Fonts
GFont tbdFont;

// *** Graphics
GBitmap* menuIcon;
GBitmap* startStopIcon;
GBitmap* nextIcon;


// *** Setup Data - Default to 26.2 miles in 4 hours with 1 mile split distance.
static int distInteger = 26;
static int distTenths = 2;
static int timeHours = 4;
static int timeMins = 0;
static int timeSecs = 0;
static int stepSize = 1; // 0 ==> 0.5

// *** Chrono Data
// Start/Stop select
#define RUN_START 0
#define RUN_STOP 1
#define RUN_MAX 2
static short chronoRunSelect = RUN_STOP;

// Chrono reset support.
static bool resetStartFired = false;
static bool resetEndFired = false;
static AppTimer *resetStartTimerHandle = NULL;
static AppTimer *resetEndTimerHandle = NULL;

// Accummulated chrono time.
static time_t chronoElapsed = 0;

// *** Pace Splits Data. Format is "nn.n hh:mm:ss" + plus newline/null.
#define MAX_SPLIT_INDEX 199
#define MAX_DISPLAY_SPLITS 4
#define CHARS_PER_SPLIT 14
static char formattedSplits[(MAX_SPLIT_INDEX + 1) * CHARS_PER_SPLIT];
static char splitsDisplayContent[MAX_DISPLAY_SPLITS * CHARS_PER_SPLIT + 1];
static int nbrSplits;
static int splitDisplayIndex = 0;

// *** Persistent Data
// Key to access persistent data for menu window.
static const uint32_t  menu_persistent_data_key = 1;

// Key to access persistent data for Pace Splits window.
static const uint32_t  ps_persistent_data_key = 2;

// Structure for menu persistent data
typedef struct menu_saved_state_S
{
  int distInteger;
  int distTenths;
  int timeHours;
  int timeMins;
  int timeSecs;
  int stepSize;
} __attribute__((__packed__)) menu_saved_state_S;

// Structure for Pace Splits persistent data
typedef struct ps_saved_state_S
{
  int chronoRunSelect;
  int chronoElapsed;
  time_t closeTm;
} __attribute__((__packed__)) ps_saved_state_S;


//##################### "Set" Common Support ################################

static void setCommonLoad(char * title) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter setCommonLoad()");
  Layer * setWindowLayer = window_get_root_layer(setWindow);
  window_set_background_color(setWindow, GColorBlack);

  setTitleLayer = text_layer_create(GRect(0, 0, 144, 32));
  text_layer_set_text_alignment(setTitleLayer, GTextAlignmentCenter);
  text_layer_set_font(setTitleLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(setTitleLayer, GColorWhite);
  text_layer_set_text_color(setTitleLayer, GColorBlack);
  text_layer_set_text(setTitleLayer, title);
  layer_add_child(setWindowLayer, text_layer_get_layer(setTitleLayer));

  setDataLayer = text_layer_create(GRect(0, 34, 144, 134));
  text_layer_set_background_color(setDataLayer, GColorWhite);
  layer_add_child(setWindowLayer, text_layer_get_layer(setDataLayer));

  nextIconLayer = bitmap_layer_create(GRect(130, 67, 14, 30));
  bitmap_layer_set_bitmap(nextIconLayer, nextIcon);
  layer_add_child(setWindowLayer, bitmap_layer_get_layer(nextIconLayer));
}


static void setCommonUnload() {
  bitmap_layer_destroy(nextIconLayer);
  text_layer_destroy(setTitleLayer);
  text_layer_destroy(setDataLayer);
  window_destroy(setWindow);
}


//##################### Pace Splits ################################

static void psGenerateSplits() {
  ////APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter psGenerateSplits()");

  //Determine number of splits needed when step size is 0.5 units.
  if (stepSize == 0)
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "calc nbrSplits - stepSize 0 path");

    if (distTenths == 0)
    {
      nbrSplits = 2 * distInteger;
    }
    else if (distTenths <= 5)
    {
      nbrSplits = 2 * distInteger + 1;
    }
    else // distTenths >= 6
    {
      nbrSplits = 2 * distInteger + 2;
    }
  }

  //Determine number of splits needed when step size is integral.
  else
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "calc nbrSplits - stepSize != 0 path");

    if (distInteger % stepSize == 0)
    {
      if (distTenths == 0)
      {
        nbrSplits = distInteger/stepSize;
      }
      else
      {
        nbrSplits = distInteger/stepSize + 1;
      }
    }
    else
    {
      nbrSplits = distInteger/stepSize + 1;
    }
  }

  ////APP_LOG(APP_LOG_LEVEL_DEBUG, "nbrSplits: %i",
  //                              nbrSplits);

  // Format all but last split using step size.
  double totalDistance = distInteger + distTenths/10.0;
  double totalSecs = timeHours * 3600 + timeMins * 60 + timeSecs;
  double realStepSize = (stepSize == 0? 0.5: stepSize);
  double timePerStep = (totalSecs/totalDistance) * realStepSize;
  for (int i = 0;
       i < nbrSplits - 1;
       i++)
  {
    // Determine split time.
    double splitTime = timePerStep * (i + 1);
    int splitTimeHour = splitTime/3600.0;
    int splitTimeMin = (splitTime - splitTimeHour * 3600.0)/60.0;
    int splitTimeSec = splitTime - splitTimeHour * 3600.0 - splitTimeMin * 60;

    // Pointer to current split.
    char * currentSplit = &(formattedSplits[i * CHARS_PER_SPLIT]);

    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "formattedSplits: %i   currentSplit: %i",
    //                             (int)&(formattedSplits[0]), (int)currentSplit);

    // Format step size = 0.5
    if (stepSize == 0)
    {
      ////APP_LOG(APP_LOG_LEVEL_DEBUG, "format splits - stepSize = 0 path");

      // Integer portion of split distance
      int splitDistInteger = (i + 1)/2;

      // Multiple is integral.
      if ((i + 1) % 2 == 0)
      {
       // Extra character for null terminater which will be overwritten by next split.
        snprintf(currentSplit, CHARS_PER_SPLIT + 1, "%2i.0 %2i:%02i:%02i\n", splitDistInteger,
                                                                               splitTimeHour,
                                                                               splitTimeMin,
                                                                               splitTimeSec);
      }

      // Instance multiple is not integral.
      else
      {
        // Extra character for null terminater which will be overwritten by next split.
        snprintf(currentSplit, CHARS_PER_SPLIT + 1, "%2i.5 %2i:%02i:%02i\n", splitDistInteger,
                                                                               splitTimeHour,
                                                                               splitTimeMin,
                                                                               splitTimeSec);
      }

      ////APP_LOG(APP_LOG_LEVEL_DEBUG, "currentSplit for step %i: %s",
      //                             i, currentSplit);
    }

    // Format integral step size
    else
    {
      ////APP_LOG(APP_LOG_LEVEL_DEBUG, "format splits - stepSize != 0 path");

      int splitDistance = stepSize * (i + 1);

      // Extra character for null terminater which will be overwritten by next split.
      snprintf(currentSplit, CHARS_PER_SPLIT + 1, " %2i %2i:%02i:%02i \n", splitDistance,
                                                                             splitTimeHour,
                                                                             splitTimeMin,
                                                                             splitTimeSec);

      ////APP_LOG(APP_LOG_LEVEL_DEBUG, "currentSplit for step %i: %s",
      //                             i, currentSplit);
    }

    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "formattedSplits after step %i: %s",
    //                             i, formattedSplits);
  }

  // *** Format last split using selected distance and time.
  
  // Pointer to last split.
  char * lastSplit = &(formattedSplits[(nbrSplits - 1) * CHARS_PER_SPLIT]);

  // Show fractional distance for last split when step size = 0.5.
  if (stepSize == 0)
  {
    snprintf(lastSplit, CHARS_PER_SPLIT, "%2i.%i %2i:%02i:00", distInteger,
                                                                        distTenths,
                                                                        timeHours,
                                                                        timeMins);

    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "stepSize = 0 lastSplit: %s",
    //                             lastSplit);
  }

  // Integral step size.
  else
  {
    // Do not show fractional distance for last split when total distance is integral.
    if(distTenths == 0)
    {
      snprintf(lastSplit, CHARS_PER_SPLIT, " %2i %2i:%02i:00 ", distInteger,
                                                                         timeHours,
                                                                         timeMins);
    }
    // Show fractional distance for last split when total distance is not integral.
    else
    {
      snprintf(lastSplit, CHARS_PER_SPLIT, "%2i.%i %2i:%02i:00", distInteger,
                                                                          distTenths,
                                                                          timeHours,
                                                                          timeMins);
    }

    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "stepSize != 0 lastSplit: %s",
    //                             lastSplit);
  }

  ////APP_LOG(APP_LOG_LEVEL_DEBUG, "formattedSplits final: %s",
  //                             formattedSplits);
}


static void ps_reset_start_handler(void *callback_data) {
  resetStartFired = true;

  text_layer_set_text(psChronoLayer, "HOLD");
}


static void ps_reset_end_handler(void *callback_data) {
  resetEndFired = true;

  chronoElapsed = 0;
  text_layer_set_text(psChronoLayer, "0:00:00");
}


static void psDisplaySelectedSplits() {

  // Get up to MAX_DISPLAY_SPLITS rows beginning at splitDisplayIndex.
  strncpy(splitsDisplayContent,
          &(formattedSplits[splitDisplayIndex * CHARS_PER_SPLIT]),
          MAX_DISPLAY_SPLITS * CHARS_PER_SPLIT);

  // Replace trailing \n with \0.
  memset(splitsDisplayContent + (MAX_DISPLAY_SPLITS * CHARS_PER_SPLIT), 0, 1);

  ////APP_LOG(APP_LOG_LEVEL_DEBUG, "splitsDisplayContent: %s",
  //                             splitsDisplayContent);

  // Set in display.
  text_layer_set_text(psSplitsLayer, splitsDisplayContent);
}


static void displayChrono(){

  static char timeText[] = "00:00:00";

  // Limit display to 2 hours digits.
  int hours = chronoElapsed/3600;
  if (hours <= 99)
  {
    int min = (chronoElapsed - hours*3600)/60;
    int sec = (chronoElapsed - hours*3600 - min*60);

    // Format.
    snprintf(timeText, sizeof(timeText), "%2i:%02i:%02i", hours, min, sec);

    text_layer_set_text(psChronoLayer, timeText);
  }
  else
  {
    text_layer_set_text(psChronoLayer, "99:59:59");
  }

}


static void ps_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Move up one position.
  if (splitDisplayIndex > 0)
  {
    splitDisplayIndex--;

    // Adjust graphics.

    psDisplaySelectedSplits();
  }
}


static void ps_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Move down one position.
  if (splitDisplayIndex < (nbrSplits - MAX_DISPLAY_SPLITS))
  {
    splitDisplayIndex++;

    // Adjust graphics.

    psDisplaySelectedSplits();
  }
}


static void ps_select_down_handler(ClickRecognizerRef recognizer, Window *window) {

  // Look for Reset if chrono is stopped.
  if (chronoRunSelect == RUN_STOP)
  {
    resetStartFired = false;
    resetStartTimerHandle = app_timer_register(500, ps_reset_start_handler, NULL);
    resetStartFired = false;
    resetEndTimerHandle = app_timer_register(1500, ps_reset_end_handler, NULL);
  }
}


static void ps_select_up_handler(ClickRecognizerRef recognizer, Window *window) {

  // Exceeded time for button press to mean start/stop toggle.
  if (resetStartFired)
  {
    resetStartFired = false;

    // Button held long enough to mean Reset.
    if (resetEndFired)
    {
      resetEndFired = false;
      chronoElapsed= 0;
    }

    // Button released to soon to do Reset.
    else
    {
      // Cancel resetEnd timer.
      app_timer_cancel(resetEndTimerHandle);
    }

    //  Restore chrono display.
    displayChrono();
  }

  // Button press means start/stop toggle
  else
  {
    // Cancel timers.
    app_timer_cancel(resetStartTimerHandle);
    app_timer_cancel(resetEndTimerHandle);

    // Toggle run state.
    chronoRunSelect = (chronoRunSelect + 1) % RUN_MAX;
  }

  // No timers are running.
  resetStartTimerHandle = NULL;
  resetEndTimerHandle = NULL;
}


static void ps_click_config_provider(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter ps_click_config_provider()");
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_REPEAT_INTERVAL, (ClickHandler) ps_up_single_click_handler);
  window_raw_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) ps_select_down_handler, (ClickHandler) ps_select_up_handler, 

NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_REPEAT_INTERVAL, (ClickHandler) ps_down_single_click_handler);
}


static void ps_handle_second_tick(struct tm *currentTime, TimeUnits units_changed) 
{
  // Maintain a running chronometer.
  if (chronoRunSelect == RUN_START)
  {
    chronoElapsed += 1;
  }

  // Display current chrono value unless displaying text for a reset.
  if (resetStartFired == false)
  {
    displayChrono();
  }
}


// Pace Splits window needs its own persistent data in order to maintain correct
// elapsed time for a running chrono.
void psRestorePersistentData(){
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter psRestorePersistentData()");
  // Restore state if exists.
  if (persist_exists(ps_persistent_data_key))
  {
    ps_saved_state_S saved_state;
    int bytes_read = 0;
    if (sizeof(ps_saved_state_S) == (bytes_read = persist_read_data(ps_persistent_data_key,
                                                                    (void *)&saved_state,
                                                                    sizeof(ps_saved_state_S))))
    {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "psRestorePersistentData() got data");
      // Chronometer time that elapsed while app was not running.
      time_t timeSinceClosed = 0;
      if (saved_state.chronoRunSelect == RUN_START)
      {
        timeSinceClosed = time(NULL) - saved_state.closeTm;
      }

      chronoRunSelect = saved_state.chronoRunSelect;
      chronoElapsed = saved_state.chronoElapsed + timeSinceClosed;
    }
    else
    {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "error during psRestorePersistentData(). bytes read = %i", bytes_read);
    }
  }
  else
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "psRestorePersistentData persist_exists() returned false");
    chronoElapsed = 0;
    chronoRunSelect = RUN_STOP;
  }
}


void psSavePersistentData(){
  ps_saved_state_S saved_state;

  saved_state.chronoRunSelect = chronoRunSelect;
  saved_state.chronoElapsed = chronoElapsed;
  saved_state.closeTm = time(NULL);

  int bytes_written = 0;
  if (sizeof(ps_saved_state_S) != (bytes_written = persist_write_data(ps_persistent_data_key,
                                                                      (void *)&saved_state,
                                                                      sizeof(ps_saved_state_S))))
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_write_data(). bytes written = %i", bytes_written);

    // Delete peristent data when a problem has occurred saving it.
    persist_delete(ps_persistent_data_key);
  }
  else
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "write state: stepSize: %i",
    //                             saved_state.stepSize);
  }
}


static void psLoad(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter psLoad()");
  psRestorePersistentData();

  Layer * psWindowLayer = window_get_root_layer(psWindow);
  window_set_background_color(psWindow, GColorBlack);

  psChronoLayer = text_layer_create(GRect(0, 0, 144, 32));
  text_layer_set_text_alignment(psChronoLayer, GTextAlignmentCenter);
  text_layer_set_font(psChronoLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(psChronoLayer, GColorWhite);
  text_layer_set_text_color(psChronoLayer, GColorBlack);
  displayChrono();
  layer_add_child(psWindowLayer, text_layer_get_layer(psChronoLayer));

  psSplitsLayer = text_layer_create(GRect(0, 34, 144, 134));
  text_layer_set_text_alignment(psSplitsLayer, GTextAlignmentCenter);
  text_layer_set_font(psSplitsLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(psSplitsLayer, GColorWhite);
  text_layer_set_text_color(psSplitsLayer, GColorBlack);
  psGenerateSplits();
  psDisplaySelectedSplits();
  layer_add_child(psWindowLayer, text_layer_get_layer(psSplitsLayer));

  ssIconLayer = bitmap_layer_create(GRect(130, 67, 14, 30));
  bitmap_layer_set_bitmap(ssIconLayer, startStopIcon);
  layer_add_child(psWindowLayer, bitmap_layer_get_layer(ssIconLayer));

  window_set_click_config_provider(psWindow, (ClickConfigProvider) ps_click_config_provider);

  // Start keeping track of chrono elapsed.
  tick_timer_service_subscribe(SECOND_UNIT, ps_handle_second_tick);
}


void psUnload(Window *window) {

  // Stop keeping track of time/chrono elapsed.
  tick_timer_service_unsubscribe();

  // Stop timers if running.
  if (resetStartTimerHandle != NULL)
  {
    app_timer_cancel(resetStartTimerHandle);
    resetStartTimerHandle = NULL;
  }
  if (resetEndTimerHandle != NULL)
  {
    app_timer_cancel(resetEndTimerHandle);
    resetEndTimerHandle = NULL;
  }

  bitmap_layer_destroy(ssIconLayer);
  text_layer_destroy(psChronoLayer);
  text_layer_destroy(psSplitsLayer);
  window_destroy(psWindow);

  psSavePersistentData();
}


//##################### Set Race Distance ################################

void rdSetIntegerDisplay(){
  static char distIntegerStr [3];
  snprintf(distIntegerStr, 3, "%2i", distInteger);
  text_layer_set_text(rdIntegerLayer, distIntegerStr);
}


void rdSetTenthsDisplay(){
  static char distTenthsStr [2];
  snprintf(distTenthsStr, 2, "%i", distTenths);
  text_layer_set_text(rdTenthsLayer, distTenthsStr);
}


static void rd_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Integer field
  if (setFieldNbr == 1)
  {
    if (distInteger == 1)
    {
      distInteger = 99;
    }
    else
    {
      distInteger--;
    }

    rdSetIntegerDisplay();
  }
  // Tenths field
  else
  {
    if (distTenths == 0)
    {
      distTenths = 9;
    }
    else
    {
      distTenths--;
    }

    rdSetTenthsDisplay();
  } 

  // Minimum cleanup to ensure Pace Splits window will make sense.
  splitDisplayIndex = 0;
}


static void rd_select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Change from integer fieldto tenths field.
  if (setFieldNbr == 1)
  {
    setFieldNbr = 2;

    layer_set_hidden(inverter_layer_get_layer(rdIntegerInverterLayer), true);
    layer_set_hidden(inverter_layer_get_layer(rdTenthsInverterLayer), false);
  }

  // Change from tenths field to integer field
  else
  {
    setFieldNbr = 1;

    layer_set_hidden(inverter_layer_get_layer(rdIntegerInverterLayer), false);
    layer_set_hidden(inverter_layer_get_layer(rdTenthsInverterLayer), true);
  }
}


static void rd_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Integer field
  if (setFieldNbr == 1)
  {
    if (distInteger == 99)
    {
      distInteger = 1;
    }
    else
    {
      distInteger++;
    }

    rdSetIntegerDisplay();
  }
  // Tenths field
  else
  {
    if (distTenths == 9)
    {
      distTenths = 0;
    }
    else
    {
      distTenths++;
    }

    rdSetTenthsDisplay();
  } 

  // Minimum cleanup to ensure Pace Splits window will make sense.
  splitDisplayIndex = 0;
}


static void rd_click_config_provider(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter rd_click_config_provider()");
  //window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) rd_up_single_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_REPEAT_INTERVAL, (ClickHandler) rd_up_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) rd_select_single_click_handler);
  //window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) rd_down_single_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_REPEAT_INTERVAL, (ClickHandler) rd_down_single_click_handler);
}


static void rdLoad(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter rdLoad()");
  setCommonLoad("Race Distance");

  // Units
  rdIntegerLayer = text_layer_create(rdIntegerGRect);
  text_layer_set_text_alignment(rdIntegerLayer, GTextAlignmentCenter);
  text_layer_set_font(rdIntegerLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(rdIntegerLayer, GColorWhite);
  text_layer_set_text_color(rdIntegerLayer, GColorBlack);
  rdSetIntegerDisplay();
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(rdIntegerLayer));

  rdIntegerInverterLayer = inverter_layer_create(rdIntegerGRect);
  layer_add_child(text_layer_get_layer(setDataLayer), inverter_layer_get_layer(rdIntegerInverterLayer));
  layer_set_hidden(inverter_layer_get_layer(rdIntegerInverterLayer), false);

  // Decimal point "."
  rdDecimalPointLayer = text_layer_create(rdIntegerDecimalPointGRect);
  text_layer_set_text_alignment(rdDecimalPointLayer, GTextAlignmentCenter);
  text_layer_set_font(rdDecimalPointLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(rdDecimalPointLayer, GColorWhite);
  text_layer_set_text_color(rdDecimalPointLayer, GColorBlack);
  text_layer_set_text(rdDecimalPointLayer, ".");
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(rdDecimalPointLayer));

  // Tenths
  rdTenthsLayer = text_layer_create(rdTenthsGRect);
  text_layer_set_text_alignment(rdTenthsLayer, GTextAlignmentCenter);
  text_layer_set_font(rdTenthsLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(rdTenthsLayer, GColorWhite);
  text_layer_set_text_color(rdTenthsLayer, GColorBlack);
  rdSetTenthsDisplay();
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(rdTenthsLayer));

  rdTenthsInverterLayer = inverter_layer_create(rdTenthsGRect);
  layer_add_child(text_layer_get_layer(setDataLayer), inverter_layer_get_layer(rdTenthsInverterLayer));
  layer_set_hidden(inverter_layer_get_layer(rdTenthsInverterLayer), true);

  // Set integer field to be updated.
  setFieldNbr = 1;

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "about to set rd_click_config_provider()");
  window_set_click_config_provider(setWindow, (ClickConfigProvider) rd_click_config_provider);
}


void rdUnload(Window *window) {

  text_layer_destroy(rdIntegerLayer);
  inverter_layer_destroy(rdIntegerInverterLayer);
  text_layer_destroy(rdDecimalPointLayer);
  text_layer_destroy(rdTenthsLayer);
  inverter_layer_destroy(rdTenthsInverterLayer);

  setCommonUnload();
}


//##################### Set Finish Time ################################

void ftSetHoursDisplay(){
  static char timeHoursStr [3];
  snprintf(timeHoursStr, 3, "%2i", timeHours);
  text_layer_set_text(ftHoursLayer, timeHoursStr);
}


void ftSetMinsDisplay(){
  static char timeMinsStr [3];
  snprintf(timeMinsStr, 3, "%i", timeMins);
  text_layer_set_text(ftMinsLayer, timeMinsStr);
}


static void ft_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Integer field
  if (setFieldNbr == 1)
  {
    if (timeHours == 0)
    {
      timeHours = 99;
    }
    else
    {
      timeHours--;
    }

    ftSetHoursDisplay();
  }
  // Tenths field
  else
  {
    if (timeMins == 0)
    {
      timeMins = 59;
    }
    else
    {
      timeMins--;
    }

    ftSetMinsDisplay();
  } 

  // Minimum cleanup to ensure Pace Splits window will make sense.
  splitDisplayIndex = 0;
}


static void ft_select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Change from integer field to tenths field.
  if (setFieldNbr == 1)
  {
    setFieldNbr = 2;

    layer_set_hidden(inverter_layer_get_layer(ftHoursInverterLayer), true);
    layer_set_hidden(inverter_layer_get_layer(ftMinsInverterLayer), false);
  }

  // Change from tenths field to integer field
  else
  {
    setFieldNbr = 1;

    layer_set_hidden(inverter_layer_get_layer(ftHoursInverterLayer), false);
    layer_set_hidden(inverter_layer_get_layer(ftMinsInverterLayer), true);
  }
}


static void ft_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  // Integer field
  if (setFieldNbr == 1)
  {
    if (timeHours == 99)
    {
      timeHours = 1;
    }
    else
    {
      timeHours++;
    }

    ftSetHoursDisplay();
  }
  // Tenths field
  else
  {
    if (timeMins == 59)
    {
      timeMins = 0;
    }
    else
    {
      timeMins++;
    }

    ftSetMinsDisplay();
  } 

  // Minimum cleanup to ensure Pace Splits window will make sense.
  splitDisplayIndex = 0;
}


static void ft_click_config_provider(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter ft_click_config_provider()");
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_REPEAT_INTERVAL, (ClickHandler) ft_up_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) ft_select_single_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_REPEAT_INTERVAL, (ClickHandler) ft_down_single_click_handler);
}


static void ftLoad(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter ftLoad()");
  setCommonLoad("Finish Time");

  // Hours
  ftHoursLayer = text_layer_create(ftHoursGRect);
  text_layer_set_text_alignment(ftHoursLayer, GTextAlignmentCenter);
  text_layer_set_font(ftHoursLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(ftHoursLayer, GColorWhite);
  text_layer_set_text_color(ftHoursLayer, GColorBlack);
  ftSetHoursDisplay();
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(ftHoursLayer));

  ftHoursInverterLayer = inverter_layer_create(ftHoursGRect);
  layer_add_child(text_layer_get_layer(setDataLayer), inverter_layer_get_layer(ftHoursInverterLayer));
  layer_set_hidden(inverter_layer_get_layer(ftHoursInverterLayer), false);

  // Colon ":"
  ftColonLayer = text_layer_create(ftColonGRect);
  text_layer_set_text_alignment(ftColonLayer, GTextAlignmentCenter);
  text_layer_set_font(ftColonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(ftColonLayer, GColorWhite);
  text_layer_set_text_color(ftColonLayer, GColorBlack);
  text_layer_set_text(ftColonLayer, ":");
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(ftColonLayer));

  // Minutes
  ftMinsLayer = text_layer_create(ftMinsGRect);
  text_layer_set_text_alignment(ftMinsLayer, GTextAlignmentCenter);
  text_layer_set_font(ftMinsLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(ftMinsLayer, GColorWhite);
  text_layer_set_text_color(ftMinsLayer, GColorBlack);
  ftSetMinsDisplay();
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(ftMinsLayer));

  ftMinsInverterLayer = inverter_layer_create(ftMinsGRect);
  layer_add_child(text_layer_get_layer(setDataLayer), inverter_layer_get_layer(ftMinsInverterLayer));
  layer_set_hidden(inverter_layer_get_layer(ftMinsInverterLayer), true);

  // Set integer field to be updated.
  setFieldNbr = 1;

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "about to set ft_click_config_provider()");
  window_set_click_config_provider(setWindow, (ClickConfigProvider) ft_click_config_provider);
}


void ftUnload(Window *window) {

  text_layer_destroy(ftHoursLayer);
  inverter_layer_destroy(ftHoursInverterLayer);
  text_layer_destroy(ftColonLayer);
  text_layer_destroy(ftMinsLayer);
  inverter_layer_destroy(ftMinsInverterLayer);

  setCommonUnload();
}


//##################### Set Split Distance ################################

void splitDistSetDisplay(){
  static char stepSizeStr [4];
  if (stepSize == 0)
  {
    snprintf(stepSizeStr, 4, "0.5");
  }
  else
  {
    snprintf(stepSizeStr, 4, "%i", stepSize);
  }
  text_layer_set_text(splitDistLayer, stepSizeStr);
}


static void sd_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  if (stepSize == 0)
  {
    stepSize = 10;
  }
  else
  {
    stepSize--;
  }

  splitDistSetDisplay();

  // Minimum cleanup to ensure Pace Splits window will make sense.
  splitDisplayIndex = 0;
}


static void sd_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  if (stepSize == 10)
  {
    stepSize = 0;
  }
  else
  {
    stepSize++;
  }

  splitDistSetDisplay();

  // Minimum cleanup to ensure Pace Splits window will make sense.
  splitDisplayIndex = 0;
}


static void sd_click_config_provider(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter sd_click_config_provider()");
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_REPEAT_INTERVAL, (ClickHandler) sd_up_single_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100,(ClickHandler) sd_down_single_click_handler);
}


static void sdLoad(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter sdLoad()");
  setCommonLoad("Split Distance");

  splitDistLayer = text_layer_create(splitDistGRect);
  text_layer_set_text_alignment(splitDistLayer, GTextAlignmentCenter);
  text_layer_set_font(splitDistLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_background_color(splitDistLayer, GColorBlack);
  text_layer_set_text_color(splitDistLayer, GColorWhite);
  splitDistSetDisplay();
  layer_add_child(text_layer_get_layer(setDataLayer), text_layer_get_layer(splitDistLayer));

  // Not needed for this option.
  layer_set_hidden(bitmap_layer_get_layer(nextIconLayer), true);

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "about to set sd_click_config_provider()");
  window_set_click_config_provider(setWindow, (ClickConfigProvider) sd_click_config_provider);
}


void sdUnload(Window *window) {

  text_layer_destroy(splitDistLayer);
  inverter_layer_destroy(splitDistInverterLayer);

  setCommonUnload();
}

//##################### Main Menu ################################

static void menuItemPaceSplits(int index, void *context){
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter menuItemPaceSplits()");
  psWindow = window_create();
  window_set_fullscreen(psWindow, true);
  window_set_window_handlers(psWindow,
                             (WindowHandlers){.load = psLoad,
                                              //.appear = psAppear,
                                              //.disappear = psDisppear,
                                              .unload = psUnload});
  window_stack_push(psWindow, true /* Animated */);
}


static void menuItemRaceDistance(int index, void *context){
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter menuItemRaceDistance()");
  setWindow = window_create();
  window_set_fullscreen(setWindow, true);
  window_set_window_handlers(setWindow,
                             (WindowHandlers){.load = rdLoad,
                                              //.appear = rdAppear,
                                              //.disappear = rdDisppear,
                                              .unload = rdUnload});
  window_stack_push(setWindow, true /* Animated */);
}


static void menuItemFinishTime(int index, void *context)
{
  setWindow = window_create();
  window_set_fullscreen(setWindow, true);
  window_set_window_handlers(setWindow,
                             (WindowHandlers){.load = ftLoad,
                                              //.appear = ftAppear,
                                              //.disappear = ftDisppear,
                                              .unload = ftUnload});
  window_stack_push(setWindow, true /* Animated */);
}


static void menuItemSplitDistance(int index, void *context)
{
  setWindow = window_create();
  window_set_fullscreen(setWindow, true);
  window_set_window_handlers(setWindow,
                             (WindowHandlers){.load = sdLoad,
                                              //.appear = sdAppear,
                                              //.disappear = sdDisppear,
                                              .unload = sdUnload});
  window_stack_push(setWindow, true /* Animated */);
}


void menuRestorePersistentData(){
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter menuRestorePersistentData()");
  // Restore state if exists.
  if (persist_exists(menu_persistent_data_key))
  {
    menu_saved_state_S saved_state;
    int bytes_read = 0;
    if (sizeof(menu_saved_state_S) == (bytes_read = persist_read_data(menu_persistent_data_key,
                                                                 (void *)&saved_state,
                                                                 sizeof(menu_saved_state_S))))
    {
      distInteger = saved_state.distInteger;
      distTenths = saved_state.distTenths;
      timeHours = saved_state.timeHours;
      timeMins = saved_state.timeMins;
      timeSecs = saved_state.timeSecs;
      stepSize = saved_state.stepSize;
    }
    else
    {
      ////APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_read_data(). bytes read = %i", bytes_read);
    }
  }
  else
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists() returned false");
  }
}


void menuSavePersistentData(){
  menu_saved_state_S saved_state;
  saved_state.distInteger = distInteger;
  saved_state.distTenths = distTenths;
  saved_state.timeHours = timeHours;
  saved_state.timeMins = timeMins;
  saved_state.timeSecs = timeSecs;
  saved_state.stepSize = stepSize;
  int bytes_written = 0;
  if (sizeof(menu_saved_state_S) != (bytes_written = persist_write_data(menu_persistent_data_key,
                                                                   (void *)&saved_state,
                                                                   sizeof(menu_saved_state_S))))
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_write_data(). bytes written = %i", bytes_written);

    // Delete peristent data when a problem has occurred saving it.
    persist_delete(menu_persistent_data_key);
  }
  else
  {
    ////APP_LOG(APP_LOG_LEVEL_DEBUG, "write state: stepSize: %i",
    //                             saved_state.stepSize);
  }
}


static void menuLoad(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Enter menuLoad()");

  menuRestorePersistentData();

  // Get graphics.
  menuIcon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON);
  startStopIcon = gbitmap_create_with_resource(RESOURCE_ID_START_STOP_ICON);
  nextIcon = gbitmap_create_with_resource(RESOURCE_ID_NEXT_ICON);

  Layer * menuWindowLayer = window_get_root_layer(menuWindow);

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "about to set menu item 0");
  menuItems[0] = (SimpleMenuItem){.title = "Pace Splits",
                                  .subtitle = NULL,
                                  .callback = menuItemPaceSplits,
                                  .icon = menuIcon};
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "menu item 0 set");
  menuItems[1] = (SimpleMenuItem){.title = "Race Distance",
                                  .subtitle = NULL,
                                  .callback = menuItemRaceDistance,
                                  .icon = NULL};
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "menu item 1 set");
  menuItems[2] = (SimpleMenuItem){.title = "Finish Time",
                                  .subtitle = NULL,
                                  .callback = menuItemFinishTime,
                                  .icon = NULL};
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "menu item 2 set");
  menuItems[3] = (SimpleMenuItem){.title = "Split Distance",
                                  .subtitle = NULL,
                                  .callback = menuItemSplitDistance,
                                  .icon = NULL};
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "menu item 3 set");
  menuItems[4] = (SimpleMenuItem){.title = APP_VERSION,
                                  .subtitle = NULL,
                                  .callback = NULL,
                                  .icon = NULL};

  menuSection[0] = (SimpleMenuSection){.items = menuItems,
                                       .num_items = NBR_MENU_ITEMS,
                                       .title = NULL};

  menuLayer = simple_menu_layer_create(layer_get_bounds(menuWindowLayer),
                                       menuWindow,
                                       menuSection,
                                       1,
                                       NULL);

  simple_menu_layer_set_selected_index(menuLayer, 0, true);
  layer_add_child(menuWindowLayer, simple_menu_layer_get_layer(menuLayer));
}


void menuUnload(Window *window) {
  simple_menu_layer_destroy(menuLayer);

  gbitmap_destroy(menuIcon);
  gbitmap_destroy(startStopIcon);
  gbitmap_destroy(nextIcon);

  menuSavePersistentData();
}


//##################### Main Program ################################
int main(void) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "main() called");
  menuWindow = window_create();
  window_set_fullscreen(menuWindow, true);
  window_set_window_handlers(menuWindow,
                             (WindowHandlers){.load = menuLoad,
                                              //.appear = menuAppear,
                                              //.disappear = menuDisppear,
                                              .unload = menuUnload});
  window_stack_push(menuWindow, true /* Animated */);
  app_event_loop();
  window_destroy(menuWindow);
}