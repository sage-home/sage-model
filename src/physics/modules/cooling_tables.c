#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cooling_tables.h"
#include "../../core/core_allvars.h"
#include "../../core/core_logging.h"

#define TABSIZE 91
#define LAST_TAB_INDEX (TABSIZE - 1)

static char *name[] = {
    "stripped_mzero.cie",
    "stripped_m-30.cie",
    "stripped_m-20.cie",
    "stripped_m-15.cie",
    "stripped_m-10.cie",
    "stripped_m-05.cie",
    "stripped_m-00.cie",
    "stripped_m+05.cie"
};

// Metallicies with respect to solar. Will be converted to absolute metallicities by adding log10(Z_sun), Zsun=0.02
static double metallicities[8] = {
    -5.0,   // actually primordial -> -infinity
    -3.0,
    -2.0,
    -1.5,
    -1.0,
    -0.5,
    +0.0,
    +0.5
};

#define NUM_METALS_TABLE sizeof(metallicities)/sizeof(metallicities[0])

static double CoolRate[NUM_METALS_TABLE][TABSIZE];

static double get_rate(int tab, double logTemp)
{
    const double dlogT = 0.05;
    const double inv_dlogT = 1.0/dlogT;

    if(logTemp < 4.0) {
        logTemp = 4.0;
    }

    int index = (int) ((logTemp - 4.0) * inv_dlogT);
    if(index >= LAST_TAB_INDEX) {
        /*MS: because index+1 is also accessed, therefore index can be at most LAST_TAB_INDEX */
        index = LAST_TAB_INDEX - 1;
    }

    const double logTindex = 4.0 + 0.05 * index;

    const double rate1 = CoolRate[tab][index];
    const double rate2 = CoolRate[tab][index + 1];

    const double rate = rate1 + (rate2 - rate1) * inv_dlogT * (logTemp - logTindex);

    return rate;
}

void read_cooling_functions(const char *root_dir)
{
    char buf[MAX_STRING_LEN];

    const double log10_zerop02 = log10(0.02);
    for(size_t i = 0; i < NUM_METALS_TABLE; i++) {
        metallicities[i] += log10_zerop02;     // add solar metallicity
    }

    for(size_t i = 0; i < NUM_METALS_TABLE; i++) {
        // Use the provided root_dir to construct the path
        snprintf(buf, MAX_STRING_LEN - 1, "%s/src/auxdata/CoolFunctions/%s", root_dir, name[i]);
        
        FILE *fd = fopen(buf, "r");
        if(fd == NULL) {
            LOG_ERROR("File '%s' not found", buf);
            ABORT(0);
        }
        
        for(int n = 0; n < TABSIZE; n++) {
            float sd_logLnorm;
            const int nitems = fscanf(fd, " %*f %*f %*f %*f %*f %f%*[^\n]", &sd_logLnorm);
            if(nitems != 1) {
                LOG_ERROR("Could not read cooling rate on line %d", n);
                ABORT(0);
            }
            CoolRate[i][n] = sd_logLnorm;
        }

        fclose(fd);
    }
}

double get_metaldependent_cooling_rate(const double logTemp, double logZ)  // pass: log10(temperature/Kelvin), log10(metallicity)
{
    if(logZ < metallicities[0])
        logZ = metallicities[0];

    if(logZ > metallicities[7])
        logZ = metallicities[7];

    int i = 0;
    while(logZ > metallicities[i + 1]) {
        i++;
    }

    // look up at i and i+1
    const double rate1 = get_rate(i, logTemp);
    const double rate2 = get_rate(i + 1, logTemp);
    const double rate = rate1 + (rate2 - rate1) / (metallicities[i + 1] - metallicities[i]) * (logZ - metallicities[i]);

    return pow(10.0, rate);
}