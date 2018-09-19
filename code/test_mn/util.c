/*
 *	utility functions
 */

#include "util.h"

#include <netlink/errno.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/route/link.h>


// From http://git.kernel.org/cgit/linux/kernel/git/jberg/iw.git/tree/util.c.
void mac_addr_n2a(char *mac_addr, unsigned char *arg)
{
	int i, l;
	l = 0;
	for (i = 0; i < ETH_NLEN; i++) 
	{
		if (i == 0) 
		{
			sprintf(mac_addr+l, "%02x", arg[i]);
			l += 2;
		} 
		else 
		{
			sprintf(mac_addr+l, ":%02x", arg[i]);
			l += 3;
		}
	}
}

int mac_addr_a2n(unsigned char *mac_addr, char *arg)
{
	int i;
	int temp;

	for (i=0; i<ETH_NLEN; i++) 
	{
		char *cp = strchr(arg, ':');
		if (cp) 
		{
			*cp = 0;
			cp++;
		}
		if (sscanf(arg, "%x", &temp) != 1)
			return -1;
		if (temp < 0 || temp > 255)
			return -1;

		mac_addr[i] = (unsigned char)temp;
		if (!cp)
			break;
		arg = cp;
	}
	if (i < ETH_NLEN - 1)
	{
//		printf("mac_addr_a2n: i[%d]\n", i);
		return -1;
	}

	return 0;
}

void print_ssid(unsigned char *ie, int ielen)
{
	uint8_t len;
	uint8_t *data;
	int i;

	while (ielen >= 2 && ielen >= ie[1]) 
	{
		if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) 
		{
			len = ie[1];
			data = ie + 2;
			for (i = 0; i < len; i++) 
			{
				if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\') 
					printf("%c", data[i]);
				else if (data[i] == ' ' && (i != 0 && i != len -1)) 
					printf(" ");
				else 
					printf("\\x%.2x", data[i]);
			}
			break;
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
}

void get_ssid(char *str, unsigned char *ie, int ielen)
{
	uint8_t len;
	uint8_t *data;
	int i;
	char *ptr = str;

	while (ielen >= 2 && ielen >= ie[1]) 
	{
		if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) 
		{
			len = ie[1];
			data = ie + 2;
			for (i = 0; i < len; i++) 
			{
				if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\') 
					sprintf(ptr++, "%c", data[i]);
				else if (data[i] == ' ' && (i != 0 && i != len -1)) 
					sprintf(ptr++, " ");
/*				else 
					sprintf(str, "\\x%.2x", data[i]);
*/			}
			break;
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
	*ptr = '\0';
}

int ieee80211_channel_to_frequency(int chan, enum nl80211_band band)
{
	/* see 802.11 17.3.8.3.2 and Annex J
	 * there are overlapping channel numbers in 5GHz and 2GHz bands */
	if (chan <= 0)
		return 0; /* not supported */
	switch (band) {
	case NL80211_BAND_2GHZ:
		if (chan == 14)
			return 2484;
		else if (chan < 14)
			return 2407 + chan * 5;
		break;
	case NL80211_BAND_5GHZ:
		if (chan >= 182 && chan <= 196)
			return 4000 + chan * 5;
		else
			return 5000 + chan * 5;
		break;
	case NL80211_BAND_60GHZ:
		if (chan < 5)
			return 56160 + chan * 2160;
		break;
	default:
		;
	}
	return 0; /* not supported */
}

int ieee80211_frequency_to_channel(int freq)
{
	/* see 802.11-2007 17.3.8.3.2 and Annex J */
	if (freq == 2484)
		return 14;
	else if (freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if (freq <= 45000) /* DMG band lower limit */
		return (freq - 5000) / 5;
	else if (freq >= 58320 && freq <= 64800)
		return (freq - 56160) / 2160;
	else
		return 0;
}

void print_ssid_escaped(const uint8_t len, const uint8_t *data)
{
	int i;

	for (i = 0; i < len; i++) {
		if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\')
			printf("%c", data[i]);
		else if (data[i] == ' ' &&
			 (i != 0 && i != len -1))
			printf(" ");
		else
			printf("\\x%.2x", data[i]);
	}
}

void error_exit(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

