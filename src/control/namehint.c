/**
 * \file control/namehint.c
 * \ingroup Configuration
 * \brief Give device name hints
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2006
 */
/*
 *  Give device name hints  - main file
 *  Copyright (c) 2006 by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "local.h"

#ifndef DOC_HIDDEN
#define DEV_SKIP	9999 /* some non-existing device number */
struct hint_list {
	char **list;
	unsigned int count;
	unsigned int allocated;
	const char *siface;
	snd_ctl_elem_iface_t iface;
	snd_ctl_t *ctl;
	snd_ctl_card_info_t *info;	
	int card;
	int device;
	long device_input;
	long device_output;
	int stream;
	int show_all;
	char *cardname;
};
#endif

static int hint_list_add(struct hint_list *list,
			 const char *name,
			 const char *description)
{
	char *x;

	if (list->count + 1 >= list->allocated) {
		char **n = realloc(list->list, (list->allocated + 10) * sizeof(char *));
		if (n == NULL)
			return -ENOMEM;
		memset(n + list->allocated, 0, 10 * sizeof(*n));
		list->allocated += 10;
		list->list = n;
	}
	if (name == NULL) {
		x = NULL;
	} else {
		x = malloc(4 + strlen(name) + (description != NULL ? (4 + strlen(description) + 1) : 0) + 1);
		if (x == NULL)
			return -ENOMEM;
		memcpy(x, "NAME", 4);
		strcpy(x + 4, name);
		if (description != NULL) {
			strcat(x, "|DESC");
			strcat(x, description);
		}
	}
	list->list[list->count++] = x;
	return 0;
}

/**
 * Add a namehint from string given in a user configuration file
 */
static int hint_list_add_custom(struct hint_list *list,
				const char *entry)
{
	int err;
	const char *sep;
	char *name;

	assert(entry);

	sep = strchr(entry, '|');
	if (sep == NULL)
		return hint_list_add(list, entry, NULL);

	name = strndup(entry, sep - entry);
	if (name == NULL)
		return -ENOMEM;

	err = hint_list_add(list, name, sep + 1);
	free(name);
	return err;
}

static void zero_handler(const char *file ATTRIBUTE_UNUSED,
			 int line ATTRIBUTE_UNUSED,
			 const char *function ATTRIBUTE_UNUSED,
			 int err ATTRIBUTE_UNUSED,
			 const char *fmt ATTRIBUTE_UNUSED,
			 va_list arg ATTRIBUTE_UNUSED)
{
}

static int get_dev_name1(struct hint_list *list, char **res, int device,
			 int stream)
{
	*res = NULL;
	if (device < 0 || device == DEV_SKIP)
		return 0;
	switch (list->iface) {
#ifdef BUILD_HWDEP
	case SND_CTL_ELEM_IFACE_HWDEP:
		{
			snd_hwdep_info_t info = {0};
			snd_hwdep_info_set_device(&info, device);
			if (snd_ctl_hwdep_info(list->ctl, &info) < 0)
				return 0;
			*res = strdup(snd_hwdep_info_get_name(&info));
			return 0;
		}
#endif
#ifdef BUILD_PCM
	case SND_CTL_ELEM_IFACE_PCM:
		{
			snd_pcm_info_t info = {0};
			snd_pcm_info_set_device(&info, device);
			snd_pcm_info_set_stream(&info, stream ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);
			if (snd_ctl_pcm_info(list->ctl, &info) < 0)
				return 0;
			switch (snd_pcm_info_get_class(&info)) {
			case SND_PCM_CLASS_MODEM:
			case SND_PCM_CLASS_DIGITIZER:
				return -ENODEV;
			default:
				break;
			}
			*res = strdup(snd_pcm_info_get_name(&info));
			return 0;
		}
#endif
#ifdef BUILD_RAWMIDI
	case SND_CTL_ELEM_IFACE_RAWMIDI:
		{
			snd_rawmidi_info_t info = {0};
			snd_rawmidi_info_set_device(&info, device);
			snd_rawmidi_info_set_stream(&info, stream ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
			if (snd_ctl_rawmidi_info(list->ctl, &info) < 0)
				return 0;
			*res = strdup(snd_rawmidi_info_get_name(&info));
			return 0;
		}
#endif
	default:
		return 0;
	}
}

static char *get_dev_name(struct hint_list *list)
{
	char *str1, *str2, *res;
	int device;
	
	device = list->device_input >= 0 ? list->device_input : list->device;
	if (get_dev_name1(list, &str1, device, 1) < 0)
		return NULL;
	device = list->device_output >= 0 ? list->device_output : list->device;
	if (get_dev_name1(list, &str2, device, 0) < 0) {
		if (str1)
			free(str1);
		return NULL;
	}
	if (str1 != NULL || str2 != NULL) {
		if (str1 != NULL && str2 != NULL) {
			if (strcmp(str1, str2) == 0) {
				res = malloc(strlen(list->cardname) + strlen(str2) + 3);
				if (res != NULL) {
					strcpy(res, list->cardname);
					strcat(res, ", ");
					strcat(res, str2);
				}
			} else {
				res = malloc(strlen(list->cardname) + strlen(str2) + strlen(str1) + 6);
				if (res != NULL) {
					strcpy(res, list->cardname);
					strcat(res, ", ");
					strcat(res, str2);
					strcat(res, " / ");
					strcat(res, str1);
				}
			}
			free(str2);
			free(str1);
			return res;
		} else {
			if (str1 != NULL) {
				str2 = "Input";
			} else {
				str1 = str2;
				str2 = "Output";
			}
			res = malloc(strlen(list->cardname) + strlen(str1) + 19);
			if (res == NULL) {
				free(str1);
				return NULL;
			}
			strcpy(res, list->cardname);
			strcat(res, ", ");
			strcat(res, str1);
			strcat(res, "|IOID");
			strcat(res, str2);
			free(str1);
			return res;
		}
	}
	/* if the specified device doesn't exist, skip this entry */
	if (list->device >= 0 || list->device_input >= 0 || list->device_output >= 0)
		return NULL;
	return strdup(list->cardname);
}

#ifndef DOC_HIDDEN
#define BUF_SIZE 128
#endif

static int try_config(snd_config_t *config,
		      struct hint_list *list,
		      const char *base,
		      const char *name)
{
	snd_local_error_handler_t eh;
	snd_config_t *res = NULL, *cfg, *cfg1, *n;
	snd_config_iterator_t i, next;
	char *buf, *buf1 = NULL, *buf2;
	const char *str;
	int err = 0, level;
	long dev = list->device;
	int cleanup_res = 0;

	list->device_input = -1;
	list->device_output = -1;
	buf = malloc(BUF_SIZE);
	if (buf == NULL)
		return -ENOMEM;
	sprintf(buf, "%s.%s", base, name);
	/* look for redirection */
	if (snd_config_search(config, buf, &cfg) >= 0 &&
	    snd_config_get_string(cfg, &str) >= 0 &&
	    ((strncmp(base, str, strlen(base)) == 0 &&
	     str[strlen(base)] == '.') || strchr(str, '.') == NULL))
	     	goto __skip_add;
	if (list->card >= 0 && list->device >= 0)
		sprintf(buf, "%s:CARD=%s,DEV=%i", name, snd_ctl_card_info_get_id(list->info), list->device);
	else if (list->card >= 0)
		sprintf(buf, "%s:CARD=%s", name, snd_ctl_card_info_get_id(list->info));
	else
		strcpy(buf, name);
	eh = snd_lib_error_set_local(&zero_handler);
	err = snd_config_search_definition(config, base, buf, &res);
	snd_lib_error_set_local(eh);
	if (err < 0)
		goto __skip_add;
	cleanup_res = 1;
	err = -EINVAL;
	if (snd_config_get_type(res) != SND_CONFIG_TYPE_COMPOUND)
		goto __cleanup;
	if (snd_config_search(res, "type", NULL) < 0)
		goto __cleanup;

#if 0	/* for debug purposes */
		{
			snd_output_t *out;
			fprintf(stderr, "********* PCM '%s':\n", buf);
			snd_output_stdio_attach(&out, stderr, 0);
			snd_config_save(res, out);
			snd_output_close(out);
			fprintf(stderr, "\n");
		}
#endif

	cfg1 = res;
	level = 0;
      __hint:
      	level++;
	if (snd_config_search(cfg1, "type", &cfg) >= 0 &&
	    snd_config_get_string(cfg, &str) >= 0 &&
	    strcmp(str, "hw") == 0) {
		if (snd_config_search(cfg1, "device", &cfg) >= 0) {
			if (snd_config_get_integer(cfg, &dev) < 0) {
				SNDERR("(%s) device must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
		}
	}
      	
	if (snd_config_search(cfg1, "hint", &cfg) >= 0) {
		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("hint (%s) must be a compound", buf);
			err = -EINVAL;
			goto __cleanup;
		}
		if (list->card < 0 &&
		    snd_config_search(cfg, "omit_noargs", &n) >= 0 &&
		    snd_config_get_bool(n) > 0)
			goto __skip_add;
		if (level == 1 &&
		    snd_config_search(cfg, "show", &n) >= 0 &&
		    snd_config_get_bool(n) <= 0)
			goto __skip_add;
		if (buf1 == NULL &&
		    snd_config_search(cfg, "description", &n) >= 0 &&
		    snd_config_get_string(n, &str) >= 0) {
			buf1 = strdup(str);
			if (buf1 == NULL) {
				err = -ENOMEM;
				goto __cleanup;
			}
		}
		if (snd_config_search(cfg, "device", &n) >= 0) {
			if (snd_config_get_integer(n, &dev) < 0) {
				SNDERR("(%s) device must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
			list->device_input = dev;
			list->device_output = dev;
		}
		if (snd_config_search(cfg, "device_input", &n) >= 0) {
			if (snd_config_get_integer(n, &list->device_input) < 0) {
				SNDERR("(%s) device_input must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
			/* skip the counterpart if only a single direction is defined */
			if (list->device_output < 0)
				list->device_output = DEV_SKIP;
		}
		if (snd_config_search(cfg, "device_output", &n) >= 0) {
			if (snd_config_get_integer(n, &list->device_output) < 0) {
				SNDERR("(%s) device_output must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
			/* skip the counterpart if only a single direction is defined */
			if (list->device_input < 0)
				list->device_input = DEV_SKIP;
		}
	} else if (level == 1 && !list->show_all)
		goto __skip_add;
	if (snd_config_search(cfg1, "slave", &cfg) >= 0 &&
	    snd_config_search(cfg, base, &cfg1) >= 0)
	    	goto __hint;
	snd_config_delete(res);
	res = NULL;
	cleanup_res = 0;
	if (strchr(buf, ':') != NULL)
		goto __ok;
	/* find, if all parameters have a default, */
	/* otherwise filter this definition */
	eh = snd_lib_error_set_local(&zero_handler);
	err = snd_config_search_alias_hooks(config, base, buf, &res);
	snd_lib_error_set_local(eh);
	if (err < 0)
		goto __cleanup;
	if (snd_config_search(res, "@args", &cfg) >= 0) {
		snd_config_for_each(i, next, cfg) {
			/* skip the argument list */
			if (snd_config_get_id(snd_config_iterator_entry(i), &str) < 0)
				continue;
			while (*str && *str >= '0' && *str <= '9') str++;
			if (*str == '\0')
				continue;
			/* the argument definition must have the default */
			if (snd_config_search(snd_config_iterator_entry(i),
					      "default", NULL) < 0) {
				err = -EINVAL;
				goto __cleanup;
			}
		}
	}
      __ok:
	err = 0;
      __cleanup:
      	if (err >= 0) {
      		list->device = dev;
 		str = list->card >= 0 ? get_dev_name(list) : NULL;
      		if (str != NULL) {
      			level = (buf1 == NULL ? 0 : strlen(buf1)) + 1 + strlen(str);
      			buf2 = realloc((char *)str, level + 1);
      			if (buf2 != NULL) {
      				if (buf1 != NULL) {
      					str = strchr(buf2, '|');
      					if (str != NULL)
						memmove(buf2 + (level - strlen(str)), str, strlen(str));
					else
						str = buf2 + strlen(buf2);
      					*(char *)str++ = '\n';
	      				memcpy((char *)str, buf1, strlen(buf1));
	      				buf2[level] = '\0';
					free(buf1);
				}
				buf1 = buf2;
			} else {
				free((char *)str);
			}
      		} else if (list->device >= 0)
      			goto __skip_add;
	      	err = hint_list_add(list, buf, buf1);
	}
      __skip_add:
	if (res && cleanup_res)
	      	snd_config_delete(res);
	if (buf1)
		free(buf1);
      	free(buf);
	return err;
}

#ifndef DOC_HIDDEN
#define IFACE(v, fcn) [SND_CTL_ELEM_IFACE_##v] = (next_devices_t)fcn

typedef int (*next_devices_t)(snd_ctl_t *, int *);

static const next_devices_t next_devices[] = {
	IFACE(CARD, NULL),
	IFACE(HWDEP, snd_ctl_hwdep_next_device),
	IFACE(MIXER, NULL),
	IFACE(PCM, snd_ctl_pcm_next_device),
	IFACE(RAWMIDI, snd_ctl_rawmidi_next_device),
	IFACE(TIMER, NULL),
	IFACE(SEQUENCER, NULL)
};
#endif

static int add_card(snd_config_t *config, snd_config_t *rw_config, struct hint_list *list, int card)
{
	int err, ok;
	snd_config_t *conf, *n;
	snd_config_iterator_t i, next;
	const char *str;
	char ctl_name[16];
	snd_ctl_card_info_t info = {0};
	int device, max_device = 0;
	
	list->info = &info;
	err = snd_config_search(config, list->siface, &conf);
	if (err < 0)
		return err;
	sprintf(ctl_name, "hw:%i", card);
	err = snd_ctl_open(&list->ctl, ctl_name, 0);
	if (err < 0)
		return err;
	err = snd_ctl_card_info(list->ctl, &info);
	if (err < 0)
		goto __error;
	snd_config_for_each(i, next, conf) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &str) < 0)
			continue;
		
		if (next_devices[list->iface] != NULL) {
			list->card = card;
			device = max_device = -1;
			err = next_devices[list->iface](list->ctl, &device);
			if (device < 0)
				err = -EINVAL;
			else
				max_device = device;
			while (err >= 0 && device >= 0) {
				err = next_devices[list->iface](list->ctl, &device);
				if (err >= 0 && device > max_device)
					max_device = device;
			}
			ok = 0;
			for (device = 0; err >= 0 && device <= max_device; device++) {
				list->device = device;
				err = try_config(rw_config, list, list->siface, str);
				if (err < 0)
					break;
				ok++;
			}
			if (ok)
				continue;
		} else {
			err = -EINVAL;
		}
		if (err == -EXDEV)
			continue;
		if (err < 0) {
			list->card = card;
			list->device = -1;
			err = try_config(rw_config, list, list->siface, str);
		}
		if (err == -ENOMEM)
			goto __error;
	}
	err = 0;
      __error:
      	snd_ctl_close(list->ctl);
	return err;
}

static int get_card_name(struct hint_list *list, int card)
{
	char scard[16], *s;
	int err;

	free(list->cardname);
	list->cardname = NULL;
	err = snd_card_get_name(card, &list->cardname);
	if (err <= 0)
		return 0;
	sprintf(scard, " #%i", card);
	s = realloc(list->cardname, strlen(list->cardname) + strlen(scard) + 1);
	if (s == NULL)
		return -ENOMEM;
	list->cardname = s;
	return 0;
}

static int add_software_devices(snd_config_t *config, snd_config_t *rw_config,
				struct hint_list *list)
{
	int err;
	snd_config_t *conf, *n;
	snd_config_iterator_t i, next;
	const char *str;

	err = snd_config_search(config, list->siface, &conf);
	if (err < 0)
		return err;
	snd_config_for_each(i, next, conf) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &str) < 0)
			continue;
		list->card = -1;
		list->device = -1;
		err = try_config(rw_config, list, list->siface, str);
		if (err == -ENOMEM)
			return -ENOMEM;
	}
	return 0;
}

/**
 * \brief Get a set of device name hints
 * \param card Card number or -1 (means all cards)
 * \param iface Interface identification (like "pcm", "rawmidi", "timer", "seq")
 * \param hints Result - array of device name hints
 * \result zero if success, otherwise a negative error code
 *
 * hints will receive a NULL-terminated array of device name hints,
 * which can be passed to #snd_device_name_get_hint to extract usable
 * values. When no longer needed, hints should be passed to
 * #snd_device_name_free_hint to release resources.
 *
 * User-defined hints are gathered from namehint.IFACE tree like:
 *
 * <code>
 * namehint.pcm [<br>
 *   myfile "file:FILE=/tmp/soundwave.raw|Save sound output to /tmp/soundwave.raw"<br>
 *   myplug "plug:front|Do all conversions for front speakers"<br>
 * ]
 * </code>
 *
 * Note: The device description is separated with '|' char.
 *
 * Special variables: defaults.namehint.showall specifies if all device
 * definitions are accepted (boolean type).
 */
int snd_device_name_hint(int card, const char *iface, void ***hints)
{
	struct hint_list list;
	char ehints[24];
	const char *str;
	snd_config_t *conf, *local_config = NULL, *local_config_rw = NULL;
	snd_config_update_t *local_config_update = NULL;
	snd_config_iterator_t i, next;
	int err;

	if (hints == NULL)
		return -EINVAL;
	err = snd_config_update_r(&local_config, &local_config_update, NULL);
	if (err < 0)
		return err;
	err = snd_config_copy(&local_config_rw, local_config);
	if (err < 0)
		return err;
	list.list = NULL;
	list.count = list.allocated = 0;
	list.siface = iface;
	list.show_all = 0;
	list.cardname = NULL;
	if (strcmp(iface, "pcm") == 0)
		list.iface = SND_CTL_ELEM_IFACE_PCM;
	else if (strcmp(iface, "rawmidi") == 0)
		list.iface = SND_CTL_ELEM_IFACE_RAWMIDI;
	else if (strcmp(iface, "timer") == 0)
		list.iface = SND_CTL_ELEM_IFACE_TIMER;
	else if (strcmp(iface, "seq") == 0)
		list.iface = SND_CTL_ELEM_IFACE_SEQUENCER;
	else if (strcmp(iface, "hwdep") == 0)
		list.iface = SND_CTL_ELEM_IFACE_HWDEP;
	else if (strcmp(iface, "ctl") == 0)
		list.iface = SND_CTL_ELEM_IFACE_MIXER;
	else {
		err = -EINVAL;
		goto __error;
	}

	if (snd_config_search(local_config, "defaults.namehint.showall", &conf) >= 0)
		list.show_all = snd_config_get_bool(conf) > 0;
	if (card >= 0) {
		err = get_card_name(&list, card);
		if (err >= 0)
			err = add_card(local_config, local_config_rw, &list, card);
	} else {
		add_software_devices(local_config, local_config_rw, &list);
		err = snd_card_next(&card);
		if (err < 0)
			goto __error;
		while (card >= 0) {
			err = get_card_name(&list, card);
			if (err < 0)
				goto __error;
			err = add_card(local_config, local_config_rw, &list, card);
			if (err < 0)
				goto __error;
			err = snd_card_next(&card);
			if (err < 0)
				goto __error;
		}
	}
	sprintf(ehints, "namehint.%s", list.siface);
	err = snd_config_search(local_config, ehints, &conf);
	if (err >= 0) {
		snd_config_for_each(i, next, conf) {
			if (snd_config_get_string(snd_config_iterator_entry(i),
						  &str) < 0)
				continue;
			err = hint_list_add_custom(&list, str);
			if (err < 0)
				goto __error;
		}
	}
	err = 0;
      __error:
	/* add an empty entry if nothing has been added yet; the caller
	 * expects non-NULL return
	 */
	if (!err && !list.list)
		err = hint_list_add(&list, NULL, NULL);
	if (err < 0)
      		snd_device_name_free_hint((void **)list.list);
	else
      		*hints = (void **)list.list;
	free(list.cardname);
	if (local_config_rw)
		snd_config_delete(local_config_rw);
	if (local_config)
		snd_config_delete(local_config);
	if (local_config_update)
		snd_config_update_free(local_config_update);
	return err;
}

/**
 * \brief Free a list of device name hints.
 * \param hints List to free
 * \result zero if success, otherwise a negative error code
 */
int snd_device_name_free_hint(void **hints)
{
	char **h;

	if (hints == NULL)
		return 0;
	h = (char **)hints;
	while (*h) {
		free(*h);
		h++;
	}
	free(hints);
	return 0;
}

/**
 * \brief Extract a value from a hint
 * \param hint A pointer to hint
 * \param id Hint value to extract ("NAME", "DESC", or "IOID", see below)
 * \result an allocated ASCII string if success, otherwise NULL
 *
 * List of valid IDs:
 * NAME - name of device
 * DESC - description of device
 * IOID - input / output identification ("Input" or "Output"), NULL means both
 *
 * The return value should be freed when no longer needed.
 */
char *snd_device_name_get_hint(const void *hint, const char *id)
{
	const char *hint1 = (const char *)hint, *delim;
	char *res;
	unsigned size;

	if (strlen(id) != 4)
		return NULL;
	while (*hint1 != '\0') {
		delim = strchr(hint1, '|');
		if (memcmp(id, hint1, 4) != 0) {
			if (delim == NULL)
				return NULL;
			hint1 = delim + 1;
			continue;
		} 
		if (delim == NULL)
			return strdup(hint1 + 4);
		size = delim - hint1 - 4;
		res = malloc(size + 1);
		if (res != NULL) {
			memcpy(res, hint1 + 4, size);
			res[size] = '\0';
		}
		return res;
	}
	return NULL;
}
