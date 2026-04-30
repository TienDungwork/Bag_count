#include "app.h"
#include <WebServer_ESP32_SC_W5500.h>

AppContext app;

AppContext& appContext() {
  return app;
}
