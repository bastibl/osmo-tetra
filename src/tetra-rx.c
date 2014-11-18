/* Test program for tetra burst synchronizer */

/* (C) 2011 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/talloc.h>

#include "tetra_common.h"
#include <phy/tetra_burst.h>
#include <phy/tetra_burst_sync.h>
#include "tetra_gsmtap.h"

// size of IO buffers (number of elements)
#define BUF_SIZE 256

void *tetra_tall_ctx;


static int process_sym_fl(float fl)
{
	int ret;

	/* very simplistic scheme */
	if (fl > 2)
		ret = 3;
	else if (fl > 0)
		ret = 1;
	else if (fl < -2)
		ret = -3;
	else
		ret = -1;

	return ret;
}

static void sym_int2bits(int sym, uint8_t *ret)
{
	switch (sym) {
	case -3:
		ret[0] = 1;
		ret[1] = 1;
		break;
	case 1:
		ret[0] = 0;
		ret[1] = 0;
		break;
	case 3:
		ret[0] = 0;
		ret[1] = 1;
		break;
	case -1:
		ret[0] = 1;
		ret[1] = 0;
		break;
	}
}

int main(int argc, char **argv)
{
	int fd;
	int opt;
	int streaming   = 0;
	int opt_verbose = 0;
	
	struct tetra_rx_state *trs;
	struct tetra_mac_state *tms;

	while ((opt = getopt(argc, argv, "sv")) != -1) {
		switch (opt) {
		case 's':
			streaming = 1;
			break;
		case 'v':
			opt_verbose = 1;
			break;

		default:
			exit(2);
		}
	}

	if (argc <= optind) {
		fprintf(stderr, "Usage: %s -s [-v] <filestream>\n", argv[0]);
		fprintf(stderr, "Usage: %s <file_with_1_byte_per_bit>\n", argv[0]);
		exit(1);
	}

	tetra_gsmtap_init("localhost", 0);

	tms = talloc_zero(tetra_tall_ctx, struct tetra_mac_state);
	tetra_mac_state_init(tms);

	trs = talloc_zero(tetra_tall_ctx, struct tetra_rx_state);
	trs->burst_cb_priv = tms;
	
	fd = open(argv[optind], O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(2);
	}

	// use old mode which uses files with 1 byte per bit
	if(streaming == 0) {
		while (1) {
			uint8_t buf[64];
			int len;

			len = read(fd, buf, sizeof(buf));
			if (len < 0) {
				perror("read");
				exit(1);
			} else if (len == 0) {
				printf("EOF");
				break;
			}
			tetra_burst_sync_in(trs, buf, len);
		}
	}
	else { // new fast mode - we will decode the incoming file ourself
		while (1) {
			int rc;
			float fl[BUF_SIZE];
			uint8_t bits[2*BUF_SIZE];
			rc = read(fd, fl, sizeof(*fl) * BUF_SIZE);
			if (rc < 0) {
				perror("read");
				exit(1);
			} else if (rc == 0) {
				break;
			}
			rc /= sizeof(*fl);
			int i;
			for (i = 0; i < rc; ++i) {
				int sym = process_sym_fl(fl[i]);
				sym_int2bits(sym, bits + i*2);
				if (opt_verbose) {
					printf("%1u%1u", bits[2*i + 0], bits[2*i + 1]);
				}
			}
			tetra_burst_sync_in(trs, bits, rc * 2);
		}
	}
	
	talloc_free(trs);
	talloc_free(tms);

	exit(0);
}
