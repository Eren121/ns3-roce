#pragma once

namespace ns3 {

/**
 * Helper to generate random values in [min, max).
 */
int GenRandomInt(int min, int max);

/**
 * Helper to generate random values in [0, max).
 */
int GenRandomInt(int max);

double GenRandomDouble(double min, double max);

}