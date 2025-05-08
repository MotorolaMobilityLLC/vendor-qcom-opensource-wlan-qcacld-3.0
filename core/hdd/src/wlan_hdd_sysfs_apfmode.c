/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <wlan_hdd_includes.h>
#include "osif_psoc_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_apfmode.h>
#include "hif.h"

static struct kobj_attribute apfmode_attribute =
	__ATTR(apfmode, 0440, hdd_sysfs_apfmode_show,
	       NULL);

int hdd_sysfs_apfmode_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &apfmode_attribute.attr);
	if (error)
		hdd_err("could not create apfmode sysfs file");

	return error;
}

void
hdd_sysfs_apfmode_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}
	sysfs_remove_file(driver_kobject, &apfmode_attribute.attr);
}

int hdd_sysfs_create_apfmode_interface(struct kobject *wifi_kobject)
{
	int error;

	if (!wifi_kobject) {
		hdd_err("could not get wifi kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(wifi_kobject,
				  &apfmode_attribute.attr);
	if (error)
		hdd_err("could not create apfmode sysfs file");

	return error;
}

void hdd_sysfs_destroy_apfmode_interface(struct kobject *wifi_kobject)
{
	if (!wifi_kobject) {
		hdd_err("could not get wifi kobject!");
		return;
	}
	sysfs_remove_file(wifi_kobject, &apfmode_attribute.attr);
}
