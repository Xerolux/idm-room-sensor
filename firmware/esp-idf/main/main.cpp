#include "dew_point.h"
extern "C" void app_main(void) { volatile float d=dew_point_c(23.0f,60.0f); (void)d; }
