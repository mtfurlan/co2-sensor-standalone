#ifndef SD_H
#define SD_H
#include <FS.h>

void teardownSD(File* logFile);
int initSD(int cs, char* logFilename, File* logFile);
#endif // SD_H
