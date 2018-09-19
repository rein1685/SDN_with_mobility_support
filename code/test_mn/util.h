/*
 *
 *   Authors:
 *    Wanjik Lee		<wjlee@pnu.edu>
 *
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#include <unistd.h>
#include <errno.h>

#include "nl80211.h"

#define ETH_NLEN 6
//#define ETH_ALEN 18

void mac_addr_n2a(char *mac_addr, unsigned char *arg);
int mac_addr_a2n(unsigned char *mac_addr, char *arg);
void print_ssid(unsigned char *ie, int ielen);
void get_ssid(char *str, unsigned char *ie, int ielen);

int ieee80211_channel_to_frequency(int chan, enum nl80211_band band);
int ieee80211_frequency_to_channel(int freq);
void print_ssid_escaped(const uint8_t len, const uint8_t *data);

void error_exit(char *message);

#endif	//UTIL_H
