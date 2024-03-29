/*
  Copyright(c) 2014-2015 Intel Corporation
  All rights reserved.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  Authors: Mengdong Lin <mengdong.lin@intel.com>
           Yao Jin <yao.jin@intel.com>
           Liam Girdwood <liam.r.girdwood@linux.intel.com>
*/

#include "tplg_local.h"

/* mapping of channel text names to types */
static const struct map_elem channel_map[] = {
	{"mono", SNDRV_CHMAP_MONO},	/* mono stream */
	{"fl", SNDRV_CHMAP_FL},		/* front left */
	{"fr", SNDRV_CHMAP_FR},		/* front right */
	{"rl", SNDRV_CHMAP_RL},		/* rear left */
	{"rr", SNDRV_CHMAP_RR},		/* rear right */
	{"fc", SNDRV_CHMAP_FC},		/* front center */
	{"lfe", SNDRV_CHMAP_LFE},	/* LFE */
	{"sl", SNDRV_CHMAP_SL},		/* side left */
	{"sr", SNDRV_CHMAP_SR},		/* side right */
	{"rc", SNDRV_CHMAP_RC},		/* rear center */
	{"flc", SNDRV_CHMAP_FLC},	/* front left center */
	{"frc", SNDRV_CHMAP_FRC},	/* front right center */
	{"rlc", SNDRV_CHMAP_RLC},	/* rear left center */
	{"rrc", SNDRV_CHMAP_RRC},	/* rear right center */
	{"flw", SNDRV_CHMAP_FLW},	/* front left wide */
	{"frw", SNDRV_CHMAP_FRW},	/* front right wide */
	{"flh", SNDRV_CHMAP_FLH},	/* front left high */
	{"fch", SNDRV_CHMAP_FCH},	/* front center high */
	{"frh", SNDRV_CHMAP_FRH},	/* front right high */
	{"tc", SNDRV_CHMAP_TC},		/* top center */
	{"tfl", SNDRV_CHMAP_TFL},	/* top front left */
	{"tfr", SNDRV_CHMAP_TFR},	/* top front right */
	{"tfc", SNDRV_CHMAP_TFC},	/* top front center */
	{"trl", SNDRV_CHMAP_TRL},	/* top rear left */
	{"trr", SNDRV_CHMAP_TRR},	/* top rear right */
	{"trc", SNDRV_CHMAP_TRC},	/* top rear center */
	{"tflc", SNDRV_CHMAP_TFLC},	/* top front left center */
	{"tfrc", SNDRV_CHMAP_TFRC},	/* top front right center */
	{"tsl", SNDRV_CHMAP_TSL},	/* top side left */
	{"tsr", SNDRV_CHMAP_TSR},	/* top side right */
	{"llfe", SNDRV_CHMAP_LLFE},	/* left LFE */
	{"rlfe", SNDRV_CHMAP_RLFE},	/* right LFE */
	{"bc", SNDRV_CHMAP_BC},		/* bottom center */
	{"blc", SNDRV_CHMAP_BLC},	/* bottom left center */
	{"brc", SNDRV_CHMAP_BRC},	/* bottom right center */
};


static int lookup_channel(const char *c)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(channel_map); i++) {
		if (strcasecmp(channel_map[i].name, c) == 0) {
			return channel_map[i].id;
		}
	}

	return -EINVAL;
}

const char *tplg_channel_name(int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(channel_map); i++) {
		if (channel_map[i].id == type)
			return channel_map[i].name;
	}

	return NULL;
}

/* Parse a channel mapping. */
int tplg_parse_channel(snd_tplg_t *tplg, snd_config_t *cfg,
		       void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_channel *channel = private;
	const char *id;
	int channel_id, value;

	if (tplg->channel_idx >= SND_SOC_TPLG_MAX_CHAN)
		return -EINVAL;

	channel += tplg->channel_idx;
	snd_config_get_id(cfg, &id);
	tplg_dbg("\tChannel %s at index %d", id, tplg->channel_idx);

	channel_id = lookup_channel(id);
	if (channel_id < 0) {
		SNDERR("invalid channel %s", id);
		return -EINVAL;
	}

	channel->id = channel_id;
	channel->size = sizeof(*channel);
	tplg_dbg("\tChan %s = %d", id, channel->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get value */
		if (tplg_get_integer(n, &value, 0) < 0)
			continue;

		if (strcmp(id, "reg") == 0)
			channel->reg = value;
		else if (strcmp(id, "shift") == 0)
			channel->shift = value;

		tplg_dbg("\t\t%s = %d", id, value);
	}

	tplg->channel_idx++;
	return 0;
}

int tplg_save_channels(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		       struct snd_soc_tplg_channel *channel,
		       unsigned int count, struct tplg_buf *dst,
		       const char *pfx)
{
	struct snd_soc_tplg_channel *c;
	const char *s;
	unsigned int index;
	int err;

	if (count == 0)
		return 0;
	err = tplg_save_printf(dst, pfx, "channel {\n");
	for (index = 0; err >= 0 && index < count; index++) {
		c = channel + index;
		s = tplg_channel_name(c->id);
		if (s == NULL)
			err = tplg_save_printf(dst, pfx, "\t%u", c->id);
		else
			err = tplg_save_printf(dst, pfx, "\t%s", s);
		if (err >= 0)
			err = tplg_save_printf(dst, NULL, " {\n");
		if (err >= 0)
			err = tplg_save_printf(dst, pfx, "\t\treg %d\n", c->reg);
		if (err >= 0 && c->shift > 0)
			err = tplg_save_printf(dst, pfx, "\t\tshift %u\n", c->shift);
		if (err >= 0)
			err = tplg_save_printf(dst, pfx, "\t}\n");
	}
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}
