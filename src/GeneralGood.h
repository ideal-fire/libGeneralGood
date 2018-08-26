#ifndef GENERALGOODSTUFF
#define GENERALGOODSTUFF

	#ifndef ISUSINGEXTENDED
		#define ISUSINGEXTENDED 0
	#endif

	#include <stdio.h>
	#include <math.h>
	#include <string.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <time.h>
	// For stuff like uint8_t
	#include <stdint.h>
	
	#if PLATFORM == PLAT_COMPUTER
		// Header for directory functions
		#include <dirent.h>
	#endif
	
	#if PLATFORM == PLAT_3DS
		#include <3ds.h>
		#include <3ds/svc.h>
		FS_Archive _sdArchive=0;
	#endif
	
	// Headers for wait function
	#if RENDERER == REND_SDL
		#include <SDL2/SDL.h>
	#elif PLATFORM == PLAT_COMPUTER
		#include <windows.h>
	#endif

	// Subplatform Stuff
	#if SUBPLATFORM == SUB_ANDROID
		// For mkdir
		#include <sys/stat.h>
		// So we can see console output with adb logcat
		#define printf SDL_Log
	#endif
	#if SUBPLATFORM == SUB_UNIX
		#include <sys/types.h>
		#include <sys/stat.h>
	#endif

	typedef uint8_t 	u8;
	typedef uint16_t 	u16;
	typedef uint32_t	u32;
	typedef uint64_t	u64;
	typedef int8_t		s8;
	typedef int16_t		s16;
	typedef int32_t		s32;
	typedef int64_t		s64;

	#if PLATFORM == PLAT_3DS
		void utf2ascii(char* dst, u16* src){
			if(!src || !dst)return;
			while(*src)*(dst++)=(*(src++))&0xFF;
			*dst=0x00;
		}
	#endif

	void generalGoodInit(){
		#if PLATFORM == PLAT_3DS
			FSUSER_OpenArchive(&_sdArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
			romfsInit();
		#elif PLATFORM == PLAT_VITA
		#elif PLATFORM == PLAT_SWITCH
			romfsInit();
		#endif
	}
	void generalGoodQuit(){
		#if PLATFORM == PLAT_3DS
			romfsExit();
		#elif PLATFORM == PLAT_VITA
		#elif PLATFORM == PLAT_COMPUTER
		#endif
	}

	// Waits for a number of miliseconds.
	void wait(int miliseconds){
		#if PLATFORM == PLAT_VITA
			sceKernelDelayThread(miliseconds*1000);
		#elif RENDERER == REND_SDL
			SDL_Delay(miliseconds);
		#elif PLATFORM == PLAT_3DS
			svcSleepThread(miliseconds*1000000);
		#elif PLATFORM == PLAT_COMPUTER
			Sleep(miliseconds);
		#elif PLATFORM == PLAT_SWITCH
			//https://github.com/switchbrew/libnx/blob/a12eb11eab9742afc8c90c0805b02fd1341a7bbc/nx/include/switch/kernel/svc.h
			svcSleepThread(miliseconds*1000000);
		#endif
	}
	
	u64 getTicks(){
		#if PLATFORM == PLAT_VITA
			// Convert to milliseconds?
			return  (sceKernelGetProcessTimeWide() / 1000);
		#elif RENDERER == REND_SDL
			return SDL_GetTicks();
		#elif PLATFORM == PLAT_3DS
			return osGetTime();
		#elif PLATFORM == PLAT_COMPUTER
			LARGE_INTEGER s_frequency;
			char s_use_qpc = QueryPerformanceFrequency(&s_frequency);
			if (s_use_qpc) {
			    LARGE_INTEGER now;
			    QueryPerformanceCounter(&now);
			    return (1000LL * now.QuadPart) / s_frequency.QuadPart;
			} else {
			    return GetTickCount();
			}
		#else
			struct timespec _myTime;
			clock_gettime(CLOCK_MONOTONIC, &_myTime);
			return _myTime.tv_nsec/1000000;
		#endif
	}

	char showErrorIfNull(void* _thingie){
		#if RENDERER == REND_SDL
			if (_thingie==NULL){
				printf("Error: %s\n",SDL_GetError());
				return 1;
			}
			return 0;
		#elif PLATFORM == PLAT_VITA || PLATFORM == PLAT_3DS
			if (_thingie==NULL){
				printf("Some wacky thingie is null");
				return 1;
			}
			return 0;
		#endif
	}

	signed char checkFileExist(const char* location){
		#if PLATFORM == PLAT_VITA
			SceUID fileHandle = sceIoOpen(location, SCE_O_RDONLY, 0777);
			if (fileHandle < 0){
				return 0;
			}else{
				sceIoClose(fileHandle);
				return 1;
			}
		#elif PLATFORM == PLAT_COMPUTER
			if( access( location, F_OK ) != -1 ) {
				return 1;
			} else {
				return 0;
			}
		#elif PLATFORM == PLAT_3DS
			FILE* fp = fopen(location,"r");
			if (fp==NULL){
				return 0;
			}else{
				fclose(fp);
				return 1;
			}
		#else
			FILE* fp = fopen(location,"r");
			if (fp==NULL){
				return 0;
			}
			fclose(fp);
			return 1;
		#endif
	}

	void createDirectory(const char* path){
		#if PLATFORM == PLAT_VITA
			sceIoMkdir(path,0777);
		#elif PLATFORM == PLAT_COMPUTER
			#if SUBPLATFORM == SUB_ANDROID || SUBPLATFORM == SUB_UNIX
				mkdir(path,0777);
			#else
				mkdir(path);
			#endif
		#elif PLATFORM == PLAT_3DS
			FSUSER_CreateDirectory(_sdArchive, fsMakePath(PATH_ASCII, path), 0);
		#endif
	}
	#ifndef ISUSINGEXTENDED
		void quitApplication(){
			#if RENDERER == REND_SDL
				SDL_Quit();
			#elif PLATFORM == PLAT_VITA
				sceKernelExitProcess(0);
			#else
				printf("No quit function avalible.");
			#endif
		}
	#endif

	/*
	================================================
	== ETC
	=================================================
	*/
	// PLEASE MAKE DIR PATHS END IN A SLASH
	#if PLATFORM == PLAT_COMPUTER
		#define CROSSDIR DIR*
		#define CROSSDIRSTORAGE struct dirent*
	#elif PLATFORM == PLAT_VITA
		#define CROSSDIR SceUID
		#define CROSSDIRSTORAGE SceIoDirent
	#elif PLATFORM == PLAT_3DS
		#define CROSSDIR Handle
		#define CROSSDIRSTORAGE FS_DirectoryEntry
	#else
		#warning NO DIRECTORY LISTING YET
		#define CROSSDIR int
		#define CROSSDIRSTORAGE int
	#endif

	char dirOpenWorked(CROSSDIR passedir){
		#if PLATFORM == PLAT_COMPUTER
			if (passedir==NULL){
				return 0;
			}
		#elif PLATFORM == PLAT_VITA
			if (passedir<0){
				return 0;
			}
		#elif PLATFORM == PLAT_3DS
			if (passedir==0){
				return 0;
			}
		#else
			return 0;
		#endif
		return 1;
	}

	CROSSDIR openDirectory(const char* filepath){
		#if PLATFORM == PLAT_COMPUTER
			return opendir(filepath);
		#elif PLATFORM == PLAT_VITA
			return (sceIoDopen(filepath));
		#elif PLATFORM == PLAT_3DS
			// Should not end in slash
			char _tempFixedFilepath[strlen(filepath)+1];
			strcpy(_tempFixedFilepath,filepath);
			if (_tempFixedFilepath[strlen(filepath)-1]==47){
				_tempFixedFilepath[strlen(filepath)-1]=0;
			}
			FS_Path _stupidPath=fsMakePath(PATH_ASCII, _tempFixedFilepath);
			CROSSDIR _openedDirectory;
			if (FSUSER_OpenDirectory(&_openedDirectory, _sdArchive, _stupidPath)!=0){
				return 0;
			}
			return _openedDirectory;
		#else
			return 0;
		#endif
	}

	char* getDirectoryResultName(CROSSDIRSTORAGE* passedStorage){
		#if PLATFORM == PLAT_COMPUTER
			return ((*passedStorage)->d_name);
		#elif PLATFORM == PLAT_VITA
			//WriteToDebugFile
			return ((passedStorage)->d_name);
		#elif PLATFORM == PLAT_3DS
			return (char*)(passedStorage->name);
		#else
			return NULL;
		#endif
	}

	// Return 0 if not work
	int directoryRead(CROSSDIR* passedir, CROSSDIRSTORAGE* passedStorage){
		#if PLATFORM == PLAT_COMPUTER
			*passedStorage = readdir (*passedir);
			if (*passedStorage != NULL){
				if (strcmp((*passedStorage)->d_name,".")==0 || strcmp((*passedStorage)->d_name,"..")==0){
					return directoryRead(passedir,passedStorage);
				}
			}
			if (*passedStorage == NULL){
				return 0;
			}else{
				return 1;
			}
		#elif PLATFORM == PLAT_VITA
			int _a = sceIoDread(*passedir,passedStorage);
			return _a;
		#elif PLATFORM == PLAT_3DS
			u32 _didReadSomething=0;
			FSDIR_Read(*passedir, &_didReadSomething, 1, passedStorage);
			if (_didReadSomething!=0){
				u16* _tempName = passedStorage->name;
				char _tempBuffer[256]; // plz no bigger
				utf2ascii(_tempBuffer,_tempName);
				int _foundStrlen = strlen(_tempBuffer);
				memcpy(_tempName,_tempBuffer,_foundStrlen+1);
			}
			return _didReadSomething;
		#else
			return 0;
		#endif
	}

	void directoryClose(CROSSDIR passedir){
		#if PLATFORM == PLAT_COMPUTER
			closedir(passedir);
		#elif PLATFORM == PLAT_VITA
			sceIoDclose(passedir);
		#elif PLATFORM == PLAT_3DS
			FSDIR_Close(passedir);
		#endif
	}

	char directoryExists(const char* filepath){
		CROSSDIR _tempdir = openDirectory(filepath);
		if (dirOpenWorked(_tempdir)==1){
			directoryClose(_tempdir);
			return 1;
		}else{
			return 0;
		}
	}
	/*
	==========================================================
	== CROSS PLATFORM FILE WRITING AND READING
	==========================================================
	*/
	#if PLATFORM == PLAT_VITA
		typedef struct{
			char* filename; // Malloc
			int internalPosition;
			FILE* fp;
		}vitaFile;
		#define CROSSFILE vitaFile

		void _fixVitaFile(vitaFile* _passedFile){
			fclose(_passedFile->fp);
			_passedFile->fp = fopen(_passedFile->filename,"rb");
			fseek(_passedFile->fp,_passedFile->internalPosition,SEEK_SET);
		}
	#elif RENDERER == REND_SDL
		#define CROSSFILE SDL_RWops
		#define CROSSFILE_START RW_SEEK_SET
		#define CROSSFILE_CUR RW_SEEK_CUR
		#define CROSSFILE_END RW_SEEK_END
	#else
		#define CROSSFILE FILE
		#define CROSSFILE_START SEEK_SET
		#define CROSSFILE_CUR SEEK_CUR
		#define CROSSFILE_END SEEK_END
	#endif

	// Removes all 0x0D and 0x0A from last two characters of string by moving null character.
	void removeNewline(char* _toRemove){
		int _cachedStrlen = strlen(_toRemove);
		if (_cachedStrlen==0){
			return;
		}
		if (_toRemove[_cachedStrlen-1]==0x0A){ // Last char is UNIX newline
			if (_cachedStrlen>=2 && _toRemove[_cachedStrlen-2]==0x0D){ // If it's a Windows newline
				_toRemove[_cachedStrlen-2]='\0';
			}else{ // Well, it's at very least a UNIX newline
				_toRemove[_cachedStrlen-1]='\0';
			}
		}
	}

	// Returns number of elements read
	size_t crossfread(void* buffer, size_t size, size_t count, CROSSFILE* stream){
		#if PLATFORM == PLAT_VITA
			size_t _readElements = fread(buffer,size,count,stream->fp);
			if (_readElements==0 && count!=0 && feof(stream->fp)==0){
				_fixVitaFile(stream);
				return crossfread(buffer,size,count,stream);
			}
			stream->internalPosition += size*_readElements;
			return _readElements;
		#elif RENDERER == REND_SDL
			return SDL_RWread(stream,buffer,size,count);
		#else
			return fread(buffer,size,count,stream);
		#endif
	}

	CROSSFILE* crossfopen(const char* filename, const char* mode){
		#if PLATFORM == PLAT_VITA
			vitaFile* _returnFile = malloc(sizeof(vitaFile));
			_returnFile->fp=fopen(filename,mode);
			_returnFile->filename = malloc(strlen(filename)+1);
				strcpy(_returnFile->filename,filename);
			_returnFile->internalPosition=0;
			return _returnFile;
		#elif RENDERER == REND_SDL
			return SDL_RWFromFile(filename,mode);
		#else
			return fopen(filename,mode);
		#endif
	}

	// Returns 0 on success.
	// Returns negative number of failure
	int crossfclose(CROSSFILE* stream){
		#if PLATFORM == PLAT_VITA
			fclose(stream->fp);
			free(stream->filename);
			free(stream);
			return 0;
		#elif RENDERER == REND_SDL
			return SDL_RWclose(stream);
		#else
			return fclose(stream);
		#endif
	}

	// stream, offset, CROSSFILE_START, CROSSFILE_END, CROSSFILE_CUR
	// For SDL, returns new position
	// Otherwise, returns 0 when it works
	int crossfseek(CROSSFILE* stream, long int offset, int origin){
		#if PLATFORM == PLAT_VITA
			int _seekReturnValue;
			//if (origin==CROSSFILE_START){
			//	_seekReturnValue = fseek(stream->fp,offset,SEEK_SET);
			//	if (_seekReturnValue==0){
			//		stream->internalPosition=offset;
			//	}
			//}else if (origin==CROSSFILE_CUR){
			//	_seekReturnValue = fseek(_passedFile->fp,offset,SEEK_CUR);
			//}else if (origin==CROSSFILE_END){
			//	_seekReturnValue = fseek(_passedFile->fp,offset,SEEK_END);
			//}else{
			//	_seekReturnValue=0;
			//}
			_seekReturnValue = fseek(stream->fp,offset,origin);
			// If seek failed but it shouldn't have because we're not at the end of the file yet.
			if (_seekReturnValue!=0 && feof(stream->fp)==0){
				_fixVitaFile(stream);
				return crossfseek(stream,offset,origin);
			}else{
				stream->internalPosition = ftell(stream->fp);
			}
			return 0;
		#elif RENDERER == REND_SDL
			return (SDL_RWseek(stream,offset,origin)==-1);
		#else
			return fseek(stream,offset,origin);
		#endif
	}

	// THIS DOES NOT DO EXACTLY WHAT IT SHOULD
	int crossungetc(int c, CROSSFILE* stream){
		if (crossfseek(stream,-1,CROSSFILE_CUR)==0){
			return c;
		}
		return EOF;
	}

	long int crossftell(CROSSFILE* fp){
		#if PLATFORM == PLAT_VITA
			return fp->internalPosition;
		#elif RENDERER == REND_SDL
			return SDL_RWseek(fp,0,CROSSFILE_CUR);
		#else
			return ftell(fp);
		#endif
	}

	// No platform specific code here
	int crossgetc(CROSSFILE* fp){
		char _readChar;
		if (crossfread(&_readChar,1,1,fp)==0){
			return EOF;
		}
		return _readChar;
	}

	char crossfeof(CROSSFILE* fp){
		#if PLATFORM == PLAT_VITA
			return feof(fp->fp);
		#elif RENDERER == REND_SDL
			if (crossgetc(fp)==EOF){
				return 1;
			}
			crossfseek(fp,-1,CROSSFILE_CUR);
			return 0;
		#else
			return feof(fp);
		#endif
	}

	// Checks if the byte is the one for a newline
	// If it's 0D, it seeks past the 0A that it assumes is next and returns 1
	signed char isNewLine(CROSSFILE* fp, unsigned char _temp){
		if (_temp==0x0D){
			//fseek(fp,1,SEEK_CUR);
			crossfseek(fp,1,CROSSFILE_CUR);
			return 1;
		}
		if (_temp=='\n' || _temp==0x0A){
			// It seems like the other newline char is skipped for me?
			return 1;
		}
		return 0;
	}

#endif