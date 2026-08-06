/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "net_tools.h"


static char seth_dev_buf[ETH_BUF_LEN];
static char sif_name[ETH_IFN_LEN];
static char sif_info[ETH_IFN_LEN * 2];

static int get_inet_cfg(char *ifname, int req, void *buf, int len)
{
	struct sockaddr_in *sin = NULL;
	struct ifreq ifr = {0};
	int sockfd = 0;
	int ret = 0;

	if (!ifname || !buf)
		return -1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("%s() create socket failed! ret:%d\n", __func__, sockfd);
		return -2;
	}

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

	ret = ioctl(sockfd, req, &ifr);
	if (ret < 0) {
		printf("%s() ioctl error! ret:%d\n", __func__, ret);
		ret = -3;
		goto ret_tag;
	}

	if (req == SIOCGIFHWADDR) {
		memcpy(buf, &ifr.ifr_ifru.ifru_hwaddr.sa_data, len);
	} else {
		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		memcpy((struct in_addr *)buf, &sin->sin_addr, len);
	}

ret_tag:
	if (sockfd >= 0)
		close(sockfd);

	return ret;
}

int get_inet_ip(char *pif_buf, int buf_len)
{
	char *pbuf = seth_dev_buf;
	struct in_addr ip_addr;
	uint32_t bytes = 0;
	char *pret = NULL;
	FILE *fd = NULL;
	int len = 0;
	int i = 0;

	if (!pif_buf)
		return -1;

	fd = fopen("/proc/net/dev", "r");
	if (!fd) {
		printf("Open \"/proc/net/dev\" failed!\n");
		return -2;
	}

	pret = fgets(pbuf, ETH_BUF_LEN, fd);
	pret = fgets(pbuf, ETH_BUF_LEN, fd);

	while (!feof(fd)) {
		pret = fgets(pbuf, ETH_BUF_LEN, fd);
		for (i = 0; i < strlen(pbuf); i++) {
			if (pbuf[i] == ':') {
				pbuf[i] = ' ';
				break;
			}
		}

		sscanf(pbuf, "%s %d", sif_name, &bytes);
		//printf("%s() sif_name:%s rx_bytes:%d\n", __func__, sif_name, bytes);

		len = strlen(sif_name);
		if (len <= 0)
			continue;

		if (bytes == 0)
			continue;

		if (strstr(sif_name, "eth")) {
			get_inet_cfg(sif_name, SIOCGIFADDR, &ip_addr, sizeof(ip_addr));

			snprintf(pif_buf, buf_len, "%s, IP:%s", sif_name, inet_ntoa(ip_addr));
			//printf("%s() OK sif_name:%s rx_bytes:%d if_info:%s\n", __func__, sif_name, bytes, pif_buf);
			break;
		}
	}

	return 0;
}
