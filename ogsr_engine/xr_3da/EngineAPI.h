// EngineAPI.h: interface for the CEngineAPI class.
//
//****************************************************************************
// Support for extension DLLs
//****************************************************************************

#pragma once

// Abstract 'Pure' class for DLL interface
class ENGINE_API DLL_Pure
{
public:
    CLASS_ID CLS_ID;

    DLL_Pure(void* params) { CLS_ID = 0; };
    DLL_Pure() { CLS_ID = 0; };
    virtual DLL_Pure* _construct() { return this; }
    virtual ~DLL_Pure(){};
};

// Class creation/destroying interface
extern "C" 
{
typedef DLL_API DLL_Pure* __cdecl Factory_Create(CLASS_ID CLS_ID);
typedef DLL_API void __cdecl Factory_Destroy(DLL_Pure* O);
};


class ENGINE_API CEngineAPI
{
private:
    HMODULE hGame;
    HMODULE hRender;
    HMODULE hTuner;

public:
    Factory_Create* pCreate;
    Factory_Destroy* pDestroy;

    void Initialize();
    void Destroy();

    CEngineAPI();
    ~CEngineAPI();
};

#define NEW_INSTANCE(a) Engine.External.pCreate(a)


