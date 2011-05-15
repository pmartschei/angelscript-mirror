#include <iostream>  // cout
#include <assert.h>  // assert()
#include <string.h>  // strstr()
#include <angelscript.h>
#include "../../../add_on/scriptbuilder/scriptbuilder.h"
#include "../../../add_on/scriptstdstring/scriptstdstring.h"
#include "../../../add_on/scriptarray/scriptarray.h"
#include "../../../add_on/scriptfile/scriptfile.h"
#include "../../../add_on/scripthelper/scripthelper.h"
#include <stdio.h>

using namespace std;

// Function prototypes
int ConfigureEngine(asIScriptEngine *engine);
int CompileScript(asIScriptEngine *engine, const char *scriptFile);
int ExecuteScript(asIScriptEngine *engine, const char *scriptFile);
void PrintString(const string &str);

void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if( msg->type == asMSGTYPE_WARNING ) 
		type = "WARN";
	else if( msg->type == asMSGTYPE_INFORMATION ) 
		type = "INFO";

	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

int main(int argc, char **argv)
{
	int r;

	// Validate the command line arguments
	bool argsValid = true;
	if( argc < 2 ) 
		argsValid = false;
	else if( argc == 2 && strcmp(argv[1], "-d") == 0 )
		argsValid = false;

	if( !argsValid )
	{
		cout << "Usage: " << endl;
		cout << "asrun [-d] <script file>" << endl;
		cout << " -d             inform if the script should be runned with deug" << endl;
		cout << " <script file>  is the script file that should be runned" << endl;
		return -1;
	}

	// Create the script engine
	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if( engine == 0 )
	{
		cout << "Failed to create script engine." << endl;
		return -1;
	}

	// The script compiler will send any compiler messages to the callback
	engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

	// Configure the script engine with all the functions, 
	// and variables that the script should be able to use.
	r = ConfigureEngine(engine);
	if( r < 0 ) return -1;
	
	// Check if the script is to be debugged
	bool debug = false;
	if( strcmp(argv[1], "-d") == 0 )
		debug = true;

	// Compile the script code
	r = CompileScript(engine, argv[debug ? 2 : 1]);
	if( r < 0 ) return -1;

	// Execute the script
	r = ExecuteScript(engine, argv[debug ? 2 : 1]);
	
	// Release the engine
	engine->Release();

	return r;
}

// This function will register the application interface
int ConfigureEngine(asIScriptEngine *engine)
{
	int r;

	RegisterStdString(engine);
	RegisterScriptArray(engine, false);

	r = engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(PrintString), asCALL_CDECL); assert( r >= 0 );
	
	return 0;
}

int CompileScript(asIScriptEngine *engine, const char *scriptFile)
{
	int r;

	// We will only initialize the global variables once we're 
	// ready to execute, so disable the automatic initialization
	engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, false);

	CScriptBuilder builder;
	r = builder.StartNewModule(engine, "script");
	if( r < 0 ) return -1;

	r = builder.AddSectionFromFile(scriptFile);
	if( r < 0 ) return -1;

	r = builder.BuildModule();
	if( r < 0 )
	{
		engine->WriteMessage(scriptFile, 0, 0, asMSGTYPE_ERROR, "Script failed to build");
		return -1;
	}

	return 0;
}

int ExecuteScript(asIScriptEngine *engine, const char *scriptFile)
{
	asIScriptModule *mod = engine->GetModule("script", asGM_ONLY_IF_EXISTS);
	if( !mod ) return -1;

	// Find the main function
	int funcId = mod->GetFunctionIdByDecl("int main()");
	if( funcId < 0 )
	{
		// Try again with "void main()"
		funcId = mod->GetFunctionIdByDecl("void main()");
	}

	if( funcId < 0 )
	{
		engine->WriteMessage(scriptFile, 0, 0, asMSGTYPE_ERROR, "Cannot find 'int main()' or 'void main()'");
		return -1;
	}

	// Once we have the main function, we first need to initialize the global variables
	int r = mod->ResetGlobalVars();
	if( r < 0 )
	{
		engine->WriteMessage(scriptFile, 0, 0, asMSGTYPE_ERROR, "Failed while initializing global variables");
		return -1;
	}

	// Set up a context to execute the script
	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(funcId);
	r = ctx->Execute();
	if( r != asEXECUTION_FINISHED )
	{
		if( r == asEXECUTION_EXCEPTION )
		{
			cout << "The script failed with an exception" << endl;
			PrintException(ctx, true);
			r = -1;
		}
		else 
		{
			cout << "The script terminated unexpectedly (" << r << ")" << endl;
			r = -1;
		}
	}
	else
	{
		// Get the return value from the script
		asIScriptFunction *func = engine->GetFunctionDescriptorById(funcId);
		if( func->GetReturnTypeId() == asTYPEID_INT32 )
		{
			r = *(int*)ctx->GetAddressOfReturnValue();
		}
		else
			r = 0;
	}
	ctx->Release();

	return r;
}

// This little function allows the script to print a string to the screen
void PrintString(const string &str)
{
	cout << str;
}
