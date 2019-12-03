#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

unsigned char buf[640*480*2]={0};

unsigned char buf_y[640*480]={0};
unsigned char buf_u[640*480]={0};
unsigned char buf_v[640*480]={0};

unsigned char rgb[3]={0};
 
int main()
{
	int i=0;
	
	FILE* fp1=NULL;
	FILE* fp2=NULL;
	fp1 = fopen("data.yuv422","rb+");
	fp2 = fopen("data.bgr","wb+");
	
	fread(buf, 640*2, 480, fp1);

	//y
	for(i=0;i<640*480;i++)
	{
		buf_y[i]=buf[2*i];
	}
	//u
	for(i=0;i<640*480/2;i++)
	{
		buf_u[2*i]=buf[4*i+1];
		buf_u[2*i+1]=buf[4*i+1];
	}
	//v
	for(i=0;i<640*480/2;i++)
	{
		buf_v[2*i]=buf[4*i+3];
		buf_v[2*i+1]=buf[4*i+3];
	}		

	int r,g,b;
	for(i=0;i<640*480;i++)
	{
		r=(int)(buf_y[i]+(1.370705*(buf_v[i]-128)));
		g=(int)(buf_y[i]-(0.698001*(buf_v[i]-128)-0.337633*(buf_u[i]-128)));
		b=(int)(buf_y[i]+(1.732446*(buf_u[i]-128)));
	
		if(r>255) r=255; if(r<0) r=0;
		if(g>255) g=255; if(g<0) g=0;
		if(b>255) b=255; if(b<0) b=0;	
		
		rgb[2] = (unsigned char)r;
		rgb[0] = (unsigned char)g;
		rgb[1] = (unsigned char)b;
		
		
		fwrite(rgb, 3, 1, fp2);
	}


    fclose(fp1);
	fclose(fp2);

    return 0;
}





