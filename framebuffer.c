#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#define MILI_SECOND_MULTIPLIER  1000000

struct sprite {
	unsigned int size;
	unsigned int width;
	unsigned int height;
	char * mem;
};



int setRGB(char *fb, int offset, char r, char g, char b)
{
	fb[offset+0] = r;
	fb[offset+1] = g;
	fb[offset+2] = b;
	return 0;
}


int parsePPM(char * file, struct sprite *holder, int line_length){
	int pic = open(file, O_RDWR);
	if (pic < 0){
		perror("Error: cannot open picture");
		exit(1);
	}
	char * mapped;
	char * index;
	struct stat s;
	char ch;
	if( fstat (pic, & s) < 0) {
		perror("Error getting fstat");
		exit(1);
	}

	int size = s.st_size;
	mapped = mmap (0, size, PROT_READ, MAP_SHARED, pic, 0);
	if (mapped == MAP_FAILED) {
		perror("Mapping failed!");
		exit(1);
	}

	if (mapped[0] != 'P' || mapped[1] != '6') {
		munmap(mapped,size);
		return -1;
	}

	int i = 3; // skipp P6
	do {
		ch = mapped[i];
		i++;
	} while(ch != 0x0A);
	int width = 0;
	int height = 0;
	int location = 0;
	if (sscanf(&mapped[i],"%d %d",&width, &height) != 2) {
		perror("Could not get image size");
		exit(1);
	}
	index = memchr(memchr(&mapped[i],0x0A,70)+1,0x0A,70)+1; // skip the comment sektion
	if (index == NULL){
		perror("???");
		exit(1);
	}

	char *buffer = malloc(width*height*4);
	if(buffer == NULL){
		perror("could not malloc, out of memory?");
		exit(1);
	}
	holder->size = width*height*4;
	holder->mem = buffer;
	holder->width = width;
	holder->height = height;

	int k = 0;
	for (int i = 0; i < width*height*3; i += 3){
		buffer[k+0] = index[i+2];	// Blau
		buffer[k+1] = index[i+1];	// Grün
		buffer[k+2] = index[i+0];	// Rot
		buffer[k+3] = 0x00;			// Alpha
		k += 4;
	}

	munmap(mapped,size);
	return 0;
}


int drawSprite(char *fb, int offset, struct sprite *holder, int line_length){
	char *location;
	for (int i = 0; i < holder->height; i++){
		// i gibt an in welcher width/ zeile wir uns befinden
		location = holder->mem+holder->width*i*4;
		memcpy(fb+offset+i*line_length,location,holder->width*4);
	}
	return 0;
}


int clearScreen(char *fb, int size){
	memset(fb,0x00,size);
	return 0;
}

int main()
{
	/* All variables that are used to open the framebuffer and map it to memory */
	int fbfd = 0; // holds the file stream to the framebuffer
	struct fb_var_screeninfo vinfo; // some info about the screen
	struct fb_fix_screeninfo finfo;
	long int screensize = 0; // length of the buffer aka screensize
	char *fbp = 0; // Holds the pointer to the memorymap-buffer
	struct timespec ts = {0, 100 * MILI_SECOND_MULTIPLIER }; // time struct, so the programm can later calculate the frames per second
	struct timeval start, end, total_t; // also used to calculate the FPS


	// Open the file for reading and writing
	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd == -1) {
		perror("Error: cannot open framebuffer device");
		exit(1);
	}
	printf("The framebuffer device was opened successfully.\n");

	// Get fixed screen information
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		perror("Error reading fixed information");
		exit(2);
	}
	// Get variable screen information
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("Error reading variable information");
		exit(3);
	}

	printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

	// Figure out the size of the screen in bytes
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	// Map the device to memory
	fbp = (char *) mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (fbp == MAP_FAILED) {
		perror("Error: failed to map framebuffer device to memory");
		exit(4);
	}
	printf("The framebuffer device was mapped to memory successfully.\n");


	int location = 500; // temp
	struct sprite temp;
	//vinfo.yres_virtual++;
	if(ioctl (fbfd, FBIOPUT_VSCREENINFO, &vinfo) == -1) perror("Hitler");
	printf("Echte auflösung %d\tvirtuelle %d\n", vinfo.yres, vinfo.yres_virtual);
	parsePPM("test4.ppm",&temp,finfo.line_length); // load a image into a sprite

	int yoffset = screensize/2; // for switching between framebuffer and backbuffer
	int run = 1; // indicates we are in the main loop and runnig. If it set to zero, the program should exit.
	while(run){
		printf("Get time\n");
		gettimeofday(&start, NULL);
		yoffset = 0;
		printf("Clear screen at address fbp+%x \n", yoffset);
		clearScreen(fbp+yoffset,screensize); // clear the screen

		location += 20;
		printf("drawsprite at fbp+%x+%x\n",yoffset,location);
		drawSprite(fbp+yoffset, location, &temp, finfo.line_length);
		if (location > 2000) location = 0;


		if (vinfo.yoffset == 0){
			vinfo.yoffset = screensize/2;
			yoffset = 0;
		} else {
			vinfo.yoffset = 0;
			yoffset = screensize/2;
		}
		ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);

		// this codeblock ensures that the game will not run faster than 30FPS
		gettimeofday(&end, NULL);
		unsigned long long t = 33-(1000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000);
		if (t < 0) t = 0;
		ts.tv_nsec = (t)*MILI_SECOND_MULTIPLIER;
		//printf("%d\n", ts.tv_nsec);
		nanosleep (&ts, NULL);
	}

	munmap(fbp, screensize);
	close(fbfd);
	return 0;
}
