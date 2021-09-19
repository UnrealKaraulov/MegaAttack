#pragma region Headers
#define _WIN32_WINNT 0x0501 
#define WINVER 0x0501 
#define NTDDI_VERSION 0x05010000
//#define BOTDEBUG
#define WIN32_LEAN_AND_MEAN
#include <stdexcept>
#include <Windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <thread>  
#pragma endregion

#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0



// Game.dll address
int GameDll = 0;
int _W3XGlobalClass = 0;
int _MoveTargetCmd = 0;
int _AttackCmd = 0;
int UseWarnIsBadReadPtr = 1;
BOOL DebugActive = FALSE;

typedef int( __fastcall * sub_6F32C880 )( int unit_item_code , int unused );
sub_6F32C880 GetTypeInfo = NULL;


BOOL IsOkayPtr( void *ptr , unsigned int size = 4 )
{
	if ( UseWarnIsBadReadPtr == 1 )
	{
		BOOL returnvalue = FALSE;
		returnvalue = IsBadReadPtr( ptr , size ) == 0;
		if ( !returnvalue )
		{
		}
		return returnvalue;
	}
	else if ( UseWarnIsBadReadPtr == 2 )
	{
		MEMORY_BASIC_INFORMATION mbi;
		if ( VirtualQuery( ptr , &mbi , sizeof( MEMORY_BASIC_INFORMATION ) ) == 0 )
		{
			return FALSE;
		}

		if ( ( int ) ptr + size > ( int ) mbi.BaseAddress + mbi.RegionSize )
		{
			return FALSE;
		}


		if ( ( int ) ptr < ( int ) mbi.BaseAddress )
		{
			return FALSE;
		}


		if ( mbi.State != MEM_COMMIT )
		{
			return FALSE;
		}


		if ( mbi.Protect != PAGE_READWRITE &&  mbi.Protect != PAGE_WRITECOPY && mbi.Protect != PAGE_READONLY )
		{
			return FALSE;
		}

		return TRUE;
	}
	else
		return TRUE;
}

union DWFP
{
	DWORD dw;
	float fl;
};

BOOL IsGame( ) // my offset + public
{
	return ( *( int* ) ( ( UINT32 ) GameDll + 0xACF678 ) > 0 || *( int* ) ( ( UINT32 ) GameDll + 0xAB62A4 ) > 0 )/* && !IsLagScreen( )*/;
}

// get thread access (for Jass natives / other functions)
void SetTlsForMe( )
{
	UINT32 Data = *( UINT32 * ) ( GameDll + 0xACEB4C );
	UINT32 TlsIndex = *( UINT32 * ) ( GameDll + 0xAB7BF4 );
	if ( TlsIndex )
	{
		UINT32 v5 = **( UINT32 ** ) ( *( UINT32 * ) ( *( UINT32 * ) ( GameDll + 0xACEB5C ) + 4 * Data ) + 44 );
		if ( !v5 || !( *( LPVOID * ) ( v5 + 520 ) ) )
		{
			Sleep( 1000 );
			SetTlsForMe( );
			return;
		}
		TlsSetValue( TlsIndex , *( LPVOID * ) ( v5 + 520 ) );
	}
	else
	{
		Sleep( 1000 );
		SetTlsForMe( );
		return;
	}
}


void TextPrint( char *szText , float fDuration )
{
	UINT32 dwDuration = *( ( UINT32 * ) &fDuration );
	UINT32 GAME_GlobalClass = GameDll + 0xAB4F80;
	UINT32 GAME_PrintToScreen = GameDll + 0x2F8E40;
	__asm
	{
		PUSH	0xFFFFFFFF
			PUSH	dwDuration
			PUSH	szText
			PUSH	0x0
			PUSH	0x0
			MOV		ECX , [ GAME_GlobalClass ]
			MOV		ECX , [ ECX ]
			CALL	GAME_PrintToScreen
	}
}



void * GetGlobalPlayerData( )
{
	if ( *( int * ) ( 0xAB65F4 + GameDll ) > 0 )
	{
		if ( IsOkayPtr( ( void* ) ( 0xAB65F4 + GameDll ) ) )
			return ( void * ) *( int* ) ( 0xAB65F4 + GameDll );
		else
			return nullptr;
	}
	else
		return nullptr;
}

int GetPlayerByNumber( int number )
{
	void * arg1 = GetGlobalPlayerData( );
	int result = -1;
	if ( arg1 != nullptr && arg1 )
	{
		result = ( int ) arg1 + ( number * 4 ) + 0x58;

		if ( IsOkayPtr( ( void* ) result ) )
		{
			result = *( int* ) result;
		}
	}
	return result;
}

int GetLocalPlayerNumber( )
{
	void * gldata = GetGlobalPlayerData( );
	if ( gldata != nullptr && gldata )
	{
		int playerslotaddr = ( int ) gldata + 0x28;
		if ( IsOkayPtr( ( void* ) playerslotaddr ) )
			return ( int ) *( short * ) ( playerslotaddr );
		else
			return -2;
	}
	else
		return -2;
}


int GetLocalPlayer( )
{
	return GetPlayerByNumber( GetLocalPlayerNumber( ) );
}




int GetSelectedUnit( )
{

	int plr = GetLocalPlayer( );
	if ( plr != -1 && plr )
	{

		int unitaddr = 0;

		__asm
		{
			MOV EAX , plr;
			MOV ECX , DWORD PTR DS : [ EAX + 0x34 ];
			MOV EAX , DWORD PTR DS : [ ECX + 0x1E0 ];
			MOV unitaddr , EAX;
		}


		if ( unitaddr > 0 )
		{
			return unitaddr;
		}
	}
	return NULL;
}


int GetUnitItemCODE( int unit_or_item_addr )
{
	return *( int* ) ( unit_or_item_addr + 0x30 );
}



char * GetUnitName( int unitaddr )
{
	int unitcode = GetUnitItemCODE( unitaddr );
	if ( unitcode > 0 )
	{

		int v3 = GetTypeInfo( unitcode , 0 );
		int v4 , v5;
		if ( v3 && ( v4 = *( int * ) ( v3 + 40 ) ) != 0 )
		{
			v5 = v4 - 1;
			if ( v5 >= ( unsigned int ) 0 )
				v5 = 0;
			return ( char * ) *( int * ) ( *( int * ) ( v3 + 44 ) + 4 * v5 );
		}
		else
		{
			return "Unnamed unit";
		}
	}
	return "Bad unit";
}



struct UnitLocation
{
	float X;
	float Y;
	float Z;
};

struct Location
{
	float X;
	float Y;
};


float DEGTORAD = 3.14159f / 180.0f;
float RADTODEG = 180.0f / 3.14159f;


#define ADDR(X,REG)\
	__asm MOV REG , DWORD PTR DS : [ X ] \
	__asm MOV REG , DWORD PTR DS : [ REG ]

void SendMoveAttackCommand( DWORD cmdId , float X , float Y )
{
	
	__asm
	{
		ADDR( _W3XGlobalClass , ECX );
		MOV ECX , DWORD PTR DS : [ ECX + 0x1B4 ];
		PUSH 0;
		PUSH 6;
		PUSH 0;
		PUSH Y;
		PUSH X;
		PUSH 0;
		PUSH cmdId;
		CALL _AttackCmd;
	}
}


void SendMoveAttackCommand( DWORD cmdId , int target )
{
	__asm
	{
		PUSH 0;
		PUSH 4;
		PUSH target;
		PUSH 0;
		PUSH cmdId;
		ADDR( _W3XGlobalClass , ECX );
		MOV ECX , DWORD PTR DS : [ ECX + 0x1B4 ];
		CALL _MoveTargetCmd;
	}
}



void GetUnitLocation( int unitaddr , float * x , float * y )
{
	*x = *( float* ) ( unitaddr + 0x284 );
	*y = *( float* ) ( unitaddr + 0x288 );
}

void GetUnitLocation3D( int unitaddr , float * x , float * y , float * z )
{
	*x = *( float* ) ( unitaddr + 0x284 );
	*y = *( float* ) ( unitaddr + 0x288 );
	*z = *( float* ) ( unitaddr + 0x28C );
}


BOOL IsNotBadUnit( int unitaddr , BOOL onlymemcheck = FALSE )
{

	if ( unitaddr > 0 && IsOkayPtr( ( void* ) unitaddr ) )
	{
		int xaddr = GameDll + 0x931934;
		int xaddraddr = ( int ) &xaddr;

		if ( *( BYTE* ) xaddraddr != *( BYTE* ) unitaddr )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 1 ) != *( BYTE* ) ( unitaddr + 1 ) )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 2 ) != *( BYTE* ) ( unitaddr + 2 ) )
			return FALSE;
		else if ( *( BYTE* ) ( xaddraddr + 3 ) != *( BYTE* ) ( unitaddr + 3 ) )
			return FALSE;

		if ( !IsOkayPtr( ( void* ) ( unitaddr + 0x5C ) ) )
		{
			return FALSE;
		}

		if ( onlymemcheck )
			return TRUE;

		unsigned int isdolbany = *( unsigned int* ) ( unitaddr + 0x5C );
		BOOL returnvalue = isdolbany != 0x1001u && ( isdolbany & 0x40000000u ) == 0;
		return returnvalue;
	}
	return FALSE;
}

int CmdWOTaddr;
void  sub_6F339C60my( int a1 , int a2 , unsigned int a3 , unsigned int a4 )
{
	CmdWOTaddr = GameDll + 0x339C60;
	__asm
	{

		PUSH a4;
		PUSH a3;
		ADDR( _W3XGlobalClass , ECX );
		MOV ECX , DWORD PTR DS : [ ECX + 0x1B4 ];
		PUSH a2;
		PUSH a1;

		CALL CmdWOTaddr;
	}
	Sleep( 2 );
}

unsigned long __stdcall AttackHackThread( LPVOID )
{

	Sleep( 5000 );
	SetTlsForMe( );

	int Unit1 = 0;
	int Unit2 = 0;

	while ( true )
	{
		if ( IsGame( ) )
		{

			if ( IsKeyPressed( VK_F6 ) )
			{
				int SelectedUnit = GetSelectedUnit( );

				if ( SelectedUnit > 0 )
				{
					char buffer[ 256 ];
					sprintf_s( buffer , 256 , "Selec first unit: %s " , GetUnitName( SelectedUnit ) );
					TextPrint( buffer , 5 );
					Unit1 = SelectedUnit;
				}

				while ( IsKeyPressed( VK_F6 ) )
				{
					Sleep( 50 );
				}
			}

			if ( IsKeyPressed( VK_F7 ) )
			{
				int SelectedUnit = GetSelectedUnit( );

				if ( SelectedUnit > 0 )
				{
					char buffer[ 256 ];
					sprintf_s( buffer , 256 , "Selec next unit: %s " , GetUnitName( SelectedUnit ) );
					TextPrint( buffer , 5 );
					Unit2 = SelectedUnit;
				}

				while ( IsKeyPressed( VK_F7 ) )
				{
					Sleep( 50 );
				}
			}


			if ( IsKeyPressed( VK_F8 ) )
			{
				TextPrint( "Stop hold F8 for start attacking!" , 5.0 );

				while ( IsKeyPressed( VK_F8 ) )
				{
					Sleep( 50 );
				}

				if ( Unit1 && Unit2 )
				{


					TextPrint( "Now select first unit and press and hold F8 key for 5 sec..." , 5.0 );

					Sleep( 3000 );

					while ( IsKeyPressed( VK_F8 ) )
					{
						DWORD cmdattack = 0xD000F;
						DWORD cmdstop = 0xD0004;
						Sleep( 150 );
						SendMoveAttackCommand( cmdattack , Unit2 );
						Sleep( 110);
						sub_6F339C60my( cmdstop , 0 , 1 , 4 );
					}
				}
				else
				{
					TextPrint( "NeedSelectTwounits!!" , 5.0 );
				}
			}

		}
		Sleep( 200 );
	}


	return 0;
}

HANDLE ttt = NULL;



BOOL WINAPI DllMain( HINSTANCE hDLL , UINT reason , LPVOID reserved )
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		GameDll = ( int ) ( GetModuleHandle( "Game.dll" ) );

		_W3XGlobalClass = GameDll + 0xAB4F80;
		_MoveTargetCmd = GameDll + 0x339D50;
		_AttackCmd = GameDll + 0x339DD0;
		GetTypeInfo = ( sub_6F32C880 ) ( GameDll + 0x32C880 );

		ttt = CreateThread( 0 , 0 , AttackHackThread , 0 , 0 , 0 );
	}
	else if ( reason == DLL_PROCESS_DETACH )
	{
		TerminateThread( ttt , 0 );
	}
	return TRUE;
}