#pragma once
#include <stdint.h>
#include <string>
#include "include/motionGloveSDKDef.h"



int GloveSuitManager_SearchByName(GloveSuitManager_TypeDef* mssm, int len, uint8_t* actorName, int nameLen);
int GloveSuitManager_getFirstUnOccupy(GloveSuitManager_TypeDef* mssm, int len);
int GloveSuitManager_AddName(GloveSuitManager_TypeDef* mssm, int len, uint8_t* actorName, int nameLen);

int isFrameCSVFormat(const char* buff);
int isFrameCSV_Glove(const char* buff);
int isFrameCSV_FullBodyMocap(const char* buff);
