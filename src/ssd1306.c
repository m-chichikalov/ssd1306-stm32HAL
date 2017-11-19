#include "ssd1306.h"

// Data buffer for screen memory
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

// A screen object
static SSD1306_t SSD1306;
// Commands array to initialize display
const static uint8_t initArray[] = {
					0xAE, //display off
					0x20, //Set Memory Addressing Mode
					0x10, //00->Horizontal Addressing Mode;
						  //01->Vertical Addressing Mode;
						  //10->Page Addressing Mode (RESET);
						  //11->Invalid
					0xB0, //Set Page Start Address for Page Addressing Mode,0-7
					0xC8, //Set COM Output Scan Direction
					0x00, //---set low column address
					0x10, //---set high column address
					0x40, //--set start line address
					0x81, //--set contrast control register
					0xFF,
					0xA1, //--set segment re-map 0 to 127
					0xA6, //--set normal display
					0xA8, //--set multiplex ratio(1 to 64)
					0x3F, //
					0xA4, //0xa4->Output follows RAM content;
						  //0xa5->Output ignores RAM content
					0xD3, //-set display offset
					0x00, //-not offset
					0xD5, //--set display clock divide ratio/oscillator frequency
					0xF0, //--set divide ratio
					0xD9, //--set pre-charge period
					0x22, //
					0xDA, //--set com pins hardware configuration
					0x12,
					0xDB, //--set vcomh
					0x20, //0x20,0.77xVcc
					0x8D, //--set DC-DC enable
					0x14, //
					0xAF //--turn on SSD1306 panel
};


//	Function for sending to the command register of display
//	Can not be used outside of this file
static void ssd1306_WriteCommand(uint8_t command) {
	HAL_I2C_Mem_Write(&hi2c1,SSD1306_I2C_ADDR,0x00,1,&command,1,10);
}

//	We should initialize screen before use it.
void ssd1306_Init(void) {
	/* Wait until the screen is definitely started */
	HAL_Delay(100);
	/* Init LCD */
	uint32_t i;
	for (i=0; i<sizeof(initArray); i++) {
		ssd1306_WriteCommand(initArray[i]);
	}
	/* Clear screen */
	ssd1306_Fill(_Black);
	/* Update screen */
	ssd1306_UpdateScreen();
	/* Set default values */
	SSD1306.CurrentX = 0;
	SSD1306.CurrentY = 0;
	/* Initialized OK */
	SSD1306.Initialized = 1;
}

//	Fill the buffer up with specific color
// 	color 	-> _Black or _White
void ssd1306_Fill(SSD1306_COLOR color) {
	/* Fill memory up */
	uint32_t i;

	for(i = 0; i < sizeof(SSD1306_Buffer); i++) {
		SSD1306_Buffer[i] = (color == _Black) ? 0x00 : 0xFF;
	}
}

//	Push the buffer into the display memory.
void ssd1306_UpdateScreen(void) {
	uint8_t i;
	
	for (i = 0; i < 8; i++) {
		ssd1306_WriteCommand(0xB0 + i);
		ssd1306_WriteCommand(0x00);
		ssd1306_WriteCommand(0x10);
		HAL_I2C_Mem_Write(&hi2c1,SSD1306_I2C_ADDR,0x40,1,
				&SSD1306_Buffer[SSD1306_WIDTH * i],SSD1306_WIDTH,100);
	}
}

//	Draw 1 pixel on the screen. (It's affected only buffer.)
//	X     -> X coordinate
//	Y     -> Y coordinate
//	color -> _Black or _White
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color) {
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
		// We are outside of the screen
		return;
	}
	if (color == _White) {
		SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
	} else {
		SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
	}
}

//	We willen 1 char naar het scherm sturen
//	ch 		  -> character
//	Font 	  -> type of using font
//	color 	  -> Black or White
//	alignment -> Left or Right
char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color, SSD1306_ALIGNMENT alignment){
	uint32_t i, b, j;
	
	// Check out if there is room for this line
	//	todo add checking for right alignment
	if (alignment == _Left) {
		if (SSD1306_WIDTH <= (SSD1306.CurrentX + Font.FontWidth) ||
			SSD1306_HEIGHT <= (SSD1306.CurrentY + Font.FontHeight))	{
			// There is no place for this character
			return 0;
		}
	} else {
		SSD1306.CurrentX -= Font.FontWidth;
	}
	
	// We go through the font
	for (i = 0; i < Font.FontHeight; i++){
		b = Font.data[(ch - 32) * Font.FontHeight + i];
		for (j = 0; j < Font.FontWidth; j++) {
			if ((b << j) & 0x8000) {
				ssd1306_DrawPixel(SSD1306.CurrentX + j,
						(SSD1306.CurrentY + i), (SSD1306_COLOR) color);
			} else {
				ssd1306_DrawPixel(SSD1306.CurrentX + j,
						(SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
			}
		}
	}
	if (alignment == _Left)
		SSD1306.CurrentX += Font.FontWidth;
	// We give the written char back for validation
	return ch;
}

//	Function prints string to the current position. (from Left to the Right)
// 	str   -> pointer to a string
//	Font  -> Type of font
//	color -> _Black or _White
char ssd1306_WriteString(char* str, FontDef Font, SSD1306_COLOR color) {
	while (*str) {
		if (ssd1306_WriteChar(*str, Font, color, _Left) != *str) {
			return *str;
		}
		str++;
	}
	// Will Return 0 if everything is fine.
	return *str;
}

//  TODO add returning some data for verification of result.
//	Function prints unsigned integer to the current position. (from Right to the Left)
// 	binaryInput   -> integer (CANNOT be more then 99 999 999)
//	Font          -> Type of font
//	color         -> _Black or _White
void ssd1306_WriteUnsignedInt(uint32_t binaryInput, FontDef Font, SSD1306_COLOR color) {
	char ch;
	if (binaryInput <= 0x05f5e0ff) {
		while (binaryInput > 0) {
			  ch =  (char)((binaryInput % 10) + 0x30);
			  binaryInput /= 10;
			  ssd1306_WriteChar(ch, Font, color, _Right);
		   }
	}
}

//  TODO add checking position???
//	Set cursor to needed position
void ssd1306_SetCursor(uint8_t x, uint8_t y) {
	SSD1306.CurrentX = x;
	SSD1306.CurrentY = y;
}




