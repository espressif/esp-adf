#ifndef _BDS_JSON_H_
#define _BDS_JSON_H_
#include "esp_log.h"
#include "nvs_flash.h"
#include "bdsc_engine.h"
typedef cJSON  BdsJson;

#define  BdsJsonType            int
#define  BDS_JSON_TYPE_NUMBER   cJSON_Number
#define  BDS_JSON_TYPE_STRING   cJSON_String
#define  BDS_JSON_TYPE_OBJECT   cJSON_Object
#define  BDS_JSON_TYPE_ARRAY    cJSON_Array

#define  BdsJsonForeach(__jObj, __tmpObj) \
     BdsJson* __tmpObj;\
    for(__tmpObj = (__jObj)->child; __tmpObj; __tmpObj=(__tmpObj)->next)

#define  BdsJsonArrayForeach   BdsJsonForeach
BdsJsonType  BdsJsonGetType( BdsJson* jObj);
BdsJson*  BdsJsonParse(const char* jStr);
int  BdsJsonPut( BdsJson* jObj);
BdsJson*  BdsJsonNext( BdsJson* jObj);
BdsJson*  BdsJsonObjectGet( BdsJson* jObj, const char* key);
char*  BdsJsonStringGet( BdsJson* jObj);
int  BdsJsonIntGet( BdsJson* jObj, int* value);
int  BdsJsonDoubleGet( BdsJson* jObj, double* value);
int  BdsJsonBoolGet( BdsJson* jObj, bool* bl);
char*  BdsJsonObjectGetString( BdsJson* jObj, const char* key);
int  BdsJsonObjectGetInt( BdsJson* jObj, const char* key, int* value);
int  BdsJsonObjectGetDouble( BdsJson* jObj, const char* key, double* value);
int  BdsJsonObjectGetBool( BdsJson* jObj, const char* key, bool* value);
int  BdsJsonArrayLen( BdsJson* jObj);
BdsJson*  BdsJsonArrayGet( BdsJson* jObj, int idx);
const char*  BdsJsonArrayGetString( BdsJson* jObj, int idx);
int  BdsJsonArrayGetInt( BdsJson* jObj, int idx, int* value);
int  BdsJsonArrayGetDouble( BdsJson* jObj, int idx, double* value);

BdsJson*  BdsJsonObjectNew();
int  BdsJsonObjectAdd( BdsJson* obj, const char* key,  BdsJson* objAdd);
int  BdsJsonObjectAddInt( BdsJson* obj, const char* key, int val);
int  BdsJsonObjectAddInt64( BdsJson* obj, const char* key, long long val);
int  BdsJsonObjectAddDouble( BdsJson* obj, const char* key, double val);
int  BdsJsonObjectAddString( BdsJson* obj, const char* key, const char* cStr);
int  BdsJsonObjectAddBool( BdsJson* obj, const char* key, bool val);
int  BdsJsonObjectDel( BdsJson* obj, const char* key);

int  BdsJsonArrayAdd( BdsJson* obj,   BdsJson* objAdd);
int  BdsJsonArrayAddInt( BdsJson* obj, int val);
int  BdsJsonArrayAddInt64( BdsJson* obj, long long val);
int  BdsJsonArrayAddDouble( BdsJson* obj, double val);
int  BdsJsonArrayAddString( BdsJson* obj, const char* cStr);
int  BdsJsonArrayAddBool( BdsJson* obj, bool val);
int  BdsJsonArrayDel( BdsJson* obj,  BdsJson* toDel);

BdsJson*  BdsJsonStringNew(const char* cStr);
BdsJson*  BdsJsonIntNew(int num);
BdsJson*  BdsJsonInt64New(long long num);
BdsJson*  BdsJsonDoubleNew(double num);
BdsJson*  BdsJsonBoolNew(bool bl);
BdsJson*  BdsJsonArrayNew();
char*  BdsJsonNewCStr( BdsJson* jObj);
char*  BdsJsonPrint( BdsJson* jObj);
char*   BdsJsonPrintUnformatted(BdsJson* jObj);

#endif
