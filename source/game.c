
/*
 * World's Trashiest Breakout Game
 * Yes, the paddle is supposed to draw wrong after reaching the end
 * of your screen.
*/

#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Each pixel is 15 bit color ( 2 bytes )
#define	SCREEN_SIZE	(SCREEN_WIDTH*SCREEN_HEIGHT)
// Color is defined by 5:5:5 RGB
#define	RGB(r,g,b)	((r)+(g<<5)+(b<<10))

#define	BLACK	RGB(0,0,0)
#define	PINK	RGB(15,0,0)
#define	BROWN	RGB(15,10,12)
#define	GRAY	RGB(31,23,26)
#define	WHITE	RGB(31,31,31)
#define	RED		RGB(31,0,0)
#define	BLUE	RGB(0,0,31)

#define	MEM_VRAM	0x06000000
#define	VID_MEM     ((u16*)MEM_VRAM)

int color_lut[] =
{
	( int ) RED,
	( int ) PINK,
	( int ) BROWN,
	( int ) GRAY,
	( int ) WHITE,
	( int ) BLUE,
};

typedef struct sPosition
{
	int x, y, width, height, color, contains;
} Position;

typedef struct sEntity
{
	Position pos;
	int destroyed;
	int transparent;
	
} Entity;

void cpu_zero_memory(void* const dst, volatile uint32_t data, uint32_t len)
{
  // Use static data for our source since this function fills in 0s every time.
  //static volatile uint32_t zero_value = 0;
  //volatile uint32_t value = data;
  volatile uint32_t* src = &data;
  // Set the fill bit flag to 1.
  len |= 1 << 24;
  __asm__ volatile(
    "ldr r0, %[src]\n" // Load address that points to 0 into first argument of CpuFastSet.
    "ldr r1, %[dst]\n" // Load the destination address into second argument of CpuFastSet.
    "mov r2, %[len]\n" // The length is the third argument to CpuFastSet.
    "swi 0xC" // 0xC is the index for CpuFastSet in the function table.
    :: [src] "m" (src), [dst] "m" (dst), [len] "r" (len)
    : "r0", "r1", "r2", "cc", "memory"
  );
}

inline void clrscrn()
{
	cpu_zero_memory( VID_MEM, 0, SCREEN_SIZE );
}

/*
 * Draws an entity at coords x,y and size w,h with color color
*/
void drawEntity( Entity* entity )
{
	if ( entity->destroyed == 1 )
	{
		return;
	}
	for ( int i = 0; i < entity->pos.width; i++ )
	{
		VID_MEM[ entity->pos.y * SCREEN_WIDTH + ( entity->pos.x + i ) ] = ( u16 ) entity->pos.color;
		VID_MEM[ ( entity->pos.y + entity->pos.height - 1 ) * SCREEN_WIDTH + ( entity->pos.x + i ) ] = ( u16 ) entity->pos.color;
	}

	for ( int i = 0; i < entity->pos.height; i++)
	{
		VID_MEM[ ( entity->pos.y + i ) * SCREEN_WIDTH + entity->pos.x ] = ( u16 ) entity->pos.color;
		VID_MEM[ ( entity->pos.y + i ) * SCREEN_WIDTH + ( entity->pos.x + entity->pos.width - 1 ) ] = ( u16 ) entity->pos.color;
	}
}

/* bounce ball y
   reverse ball.y speed
   randomize ball.x speed
*/
void bounceBallY( int* ball_speed_x, int* ball_speed_y )
{
	*ball_speed_y = -( *ball_speed_y );
	*ball_speed_x = ( rand() % 6 ) - 3;
}

/* bounce ball x
   reverse ball.x speed
*/
void bounceBallX( int* ball_speed_x )
{
	*ball_speed_x = -( *ball_speed_x );
}

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main( void )
{
	int paddle_speed_scale = 6;
	int block_number = 100;

	// set up player paddle
	Entity paddle = { {0, 5, SCREEN_HEIGHT - 5, 25, ( int ) RED, -1 }, 0, 0 };

	// set up ball
	Entity ball = { {40, 40, 5, 5, ( int ) GRAY, -1 }, 0, 0 };

	// Initialize blocks array
	Entity blocks[ block_number ];

	// Populate blocks array
	int block_width = 15;
	int block_height = 5;

	int temp_x = 0;
	int temp_y = 100 - block_height;

	for ( int i = 0; i < block_number; ++i )
	{
		if ( temp_x * block_width >= SCREEN_WIDTH )
		{
			temp_x = 0;
			temp_y = temp_y + block_height;
		}
		// ---------- FIX FOR WRAPAROUND ------------------
		blocks[ i ].pos.x = temp_x * block_width;
		// ---------- FIX FOR WRAPAROUND ------------------
		
		blocks[ i ].pos.y = block_height + temp_y;

		blocks[ i ].pos.width = block_width;
		blocks[ i ].pos.height = block_height;
		blocks[ i ].pos.color = color_lut[ i % ( sizeof( color_lut ) / sizeof( int ) ) ];
		blocks[ i ].pos.contains = 0;
		blocks[ i ].destroyed = 0;
		blocks[ i ].transparent = 0;
		++temp_x;
	}

	// Initialize ball speed variables

	int ball_speed_x = ( rand() % 6 ) - 3;
	int ball_speed_y = abs( rand() % 3 ) + 1;

	// Bounce ball ( from paddle )
	bounceBallY( &ball_speed_x, &ball_speed_y );

	// Set up the interrupt handlers
	irqInit();

	// Enable Vblank Interrupt to allow VblankIntrWait
	irqEnable( IRQ_VBLANK );

	// Allow Interrupts
	REG_IME = 1;
	REG_DISPCNT = MODE_3 | BG2_ENABLE;

	setRepeat( 10, 2 );

	clrscrn();

	while (1)
	{
		int keys;
		VBlankIntrWait();
		scanKeys();

		keys = keysDownRepeat();

		if ( keys & KEY_LEFT )
		{
			clrscrn();
			if ( paddle.pos.x != 0 )
			{
				paddle.pos.x -= paddle_speed_scale;
			}
		}
		if ( keys & KEY_RIGHT )
		{
			clrscrn();
			if ( ( paddle.pos.x + paddle.pos.width ) != SCREEN_WIDTH )
			{
				paddle.pos.x += paddle_speed_scale;
			}
		}

		// Update position of the ball
		ball.pos.x += ball_speed_x;
		ball.pos.y += ball_speed_y;

		// Collision detection of ball:
		//		If ball.y == SCREEN_HEIGHT
		//			bounce ball
		if ( ball.pos.y >= SCREEN_HEIGHT )
		{
			bounceBallY( &ball_speed_x, &ball_speed_y );
		}
		//		If ball.y and ball.x close enough to paddle
		//			bounce ball
		if ( ( ball.pos.y <= ( paddle.pos.y + paddle.pos.height ) ) && ( ball.pos.x <= ( paddle.pos.x + paddle.pos.width ) ) && ( ball.pos.x >= paddle.pos.x ) )
		{
			bounceBallY( &ball_speed_x, &ball_speed_y );
		}

		//		If ball.x hits walls
		//			bounce on x
		if ( ( ball.pos.x >= SCREEN_WIDTH ) || ( ball.pos.x <= 0 ) )
		{
			bounceBallX( &ball_speed_x );
		}

		// Check ball's Y position to see if it hits a brick
		for ( int i = 0; i < block_number; ++i )
		{
			if ( blocks[ i ].destroyed == 1 )
			{
				continue;
			}
			if ( ( ball.pos.y + ball.pos.height ) >= blocks[ i ].pos.y )
			{
				if ( ball.pos.y <= ( blocks[ i ].pos.y + blocks[ i ].pos.height ) )
				{
					if ( ( ball.pos.x + ball.pos.width ) >= blocks[ i ].pos.x )
					{
						if ( ball.pos.x <= ( blocks[ i ].pos.x + blocks[ i ].pos.width ) )
						{
							// hit brick
							blocks[ i ].destroyed = 1;
							bounceBallY( &ball_speed_x, &ball_speed_y );
							blocks[ i ].transparent = 1;
						}
					}
				}
			}
		}


		drawEntity( &paddle );

		drawEntity( &ball );

		for ( int i = 0; i < block_number; ++i )
		{
			drawEntity( &blocks[ i ] );
		}
	};
}


