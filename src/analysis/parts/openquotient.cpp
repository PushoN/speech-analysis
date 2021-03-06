#include "../Analyser.h"
#include "GCOI/GCOI.h"

void Analyser::analyseOq()
{
    if (lastPitchFrame != 0.0) {
        std::vector<GCOI::GIPair> giPairs = GCOI::estimate_MultiProduct(x, fs, 3);
        //std::vector<GCOI::GIPair> giPairs = GCOI::estimate_Sedreams(x, fs, lastPitchFrame);

        lastOqFrame = GCOI::estimateOq(giPairs);
    }
    else {
        lastOqFrame = 0.0;
    }
}
