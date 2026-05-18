/* doppler_lib — combined C library entry point.
 * Component symbols are provided by OBJECT libraries linked via
 * target_sources; only the version string lives here.
 */
#include "doppler/version.h"

const char *
doppler_version(void)
{
    return DOPPLER_VERSION;
}
