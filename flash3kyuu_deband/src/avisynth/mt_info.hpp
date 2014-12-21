#pragma once

#include <Windows.h>

#include "avisynth.h"

#include <assert.h>
#include <stdlib.h>

typedef struct _mt_info
{
    int n;
	PVideoFrame src;
	PVideoFrame dst;
	unsigned char *dstp_u;
	unsigned char *dstp_v;
	IScriptEnvironment* env;

	bool exit;

	HANDLE thread_handle;
	HANDLE work_event;
	HANDLE work_complete_event;
} mt_info;

static mt_info* mt_info_create(void) {
	mt_info* ret = (mt_info*)malloc(sizeof(mt_info));
	if (ret) {
		memset(ret, 0, sizeof(mt_info));
		ret->work_complete_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		ret->work_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!ret->work_complete_event || !ret->work_event) {
			CloseHandle(ret->work_complete_event);
			CloseHandle(ret->work_event);
			free(ret);
			ret = NULL;
		}
	}
	return ret;
}

static void mt_info_reset_pointers(volatile mt_info* info) {
	assert(info);

	info->dstp_u = NULL;
	info->dstp_v = NULL;
	info->src = NULL;
	info->dst = NULL;
}

static void mt_info_destroy(volatile mt_info* info) {
	if (!info) {
		return;
	}
	
	assert(info->work_complete_event);
	assert(info->work_event);

	mt_info_reset_pointers(info);
	
	CloseHandle(info->work_complete_event);
	CloseHandle(info->work_event);
	CloseHandle(info->thread_handle);

	free((void*)info);
}