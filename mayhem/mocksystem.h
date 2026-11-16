// Minimal IBaseSystem for fuzzing the HLTV parsers outside the real HLTV process.
// Mirrors the real System's observable behaviour (HLTV/Console/src/System.cpp): Errorf
// reports and returns (the real one calls Stop(), it does not terminate), LoadFile hands out
// a malloc'd copy with one spare byte, and the clock is constant so results depend on the input only.
#pragma once

#include "precompiled.h"

#include <stdlib.h>
#include <string.h>

class MockSystem : public IBaseSystem {
public:
	MockSystem() : m_fileData(nullptr), m_fileSize(0) {}
	virtual ~MockSystem() {}

	void SetFile(const uint8_t *data, size_t size) { m_fileData = data; m_fileSize = size; }

	// ISystemModule
	virtual bool Init(IBaseSystem *, int, char *) { return true; }
	virtual void RunFrame(double) {}
	virtual void ReceiveSignal(ISystemModule *, unsigned int, void *) {}
	virtual void ExecuteCommand(int, char *) {}
	virtual void RegisterListener(ISystemModule *) {}
	virtual void RemoveListener(ISystemModule *) {}
	virtual IBaseSystem *GetSystem() { return this; }
	virtual int GetSerial() { return 0; }
	virtual char *GetStatusLine() { return (char *)""; }
	virtual char *GetType() { return (char *)"mock"; }
	virtual char *GetName() { return (char *)"mock"; }
	virtual int GetState() { return 0; }
	virtual int GetVersion() { return 0; }
	virtual void ShutDown() {}

	// IBaseSystem
	virtual double GetTime() { return 1.0; }
	virtual unsigned int GetTick() { return 0; }
	virtual void SetFPS(float) {}
	virtual void Printf(char *, ...) {}
	virtual void DPrintf(char *, ...) {}
	virtual void RedirectOutput(char *, int) {}
	virtual IFileSystem *GetFileSystem() { return nullptr; }
	virtual unsigned char *LoadFile(const char *, int *length)
	{
		unsigned char *copy = (unsigned char *)malloc(m_fileSize + 1);
		if (!copy)
			return nullptr;
		if (m_fileSize)
			memcpy(copy, m_fileData, m_fileSize);
		copy[m_fileSize] = 0;
		if (length)
			*length = (int)m_fileSize;
		return copy;
	}
	virtual void FreeFile(unsigned char *fileHandle) { free(fileHandle); }
	virtual void SetTitle(char *) {}
	virtual void SetStatusLine(char *) {}
	virtual void ShowConsole(bool) {}
	virtual void LogConsole(char *) {}
	virtual bool InitVGUI(IVGuiModule *) { return false; }
	virtual bool RegisterCommand(char *, ISystemModule *, int) { return true; }
	virtual void GetCommandMatches(char *, ObjectList *) {}
	virtual void ExecuteString(const char *) {}
	virtual void ExecuteFile(char *) {}
	virtual void Errorf(char *, ...) {}
	virtual char *CheckParam(char *) { return nullptr; }
	virtual bool AddModule(ISystemModule *, char *) { return true; }
	virtual ISystemModule *GetModule(char *, char *, char *) { return nullptr; }
	virtual bool RemoveModule(ISystemModule *) { return true; }
	virtual void Stop() {}
	virtual char *GetBaseDir() { return (char *)"."; }

private:
	const uint8_t *m_fileData;
	size_t m_fileSize;
};
