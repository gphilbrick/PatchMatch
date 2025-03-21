/*
General utility kernels that might be used by anything
*/


__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;


//An external distance map only records distances from points outside the shape
//to the shape.  This kernel sets up initialMap prior to
//running jumpflood.  inShapeDist should normally be zero, of course, but we allow
//it to be other stuff in order to allow "staggered distances".
__kernel void externalDistanceMapInit(
        global int* shape,
        global float* initialMap,
        float inShapeDist
)
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int width = get_global_size(0);
    int idx = x+width*y;
    initialMap[idx] = shape[x+width*y] ? inShapeDist : MAXFLOAT;
}

__kernel void externalDistanceMapStep(
            global int* shape, //const
            global float* prevDistMap,
            global float* nextDistMap,
            int k)
{
    int width = get_global_size(0);
    int height = get_global_size(1);
    int x=get_global_id(0);
    int y=get_global_id(1);

    if(shape[x+width*y])
    {
        nextDistMap[x+width*y]=prevDistMap[x+width*y];
        return;
    }

    float smallestDist = prevDistMap[x+width*y];

    for(int i=-1; i<=1; i++)
    {
        for(int j=-1; j<=1; j++)
        {
            if(i==0 && j==0) continue;
            int candidateX=x+k*i;
            int candidateY=y+k*j;
            if(candidateX<0 || candidateY<0 ||
               candidateX>=width || candidateY>=height) continue;
            int candidateIdx = candidateX + candidateY*width;
            float candidateDist = prevDistMap[candidateIdx] +
                                abs(i*k) + abs(j*k);
            if(candidateDist<smallestDist)
            {
                smallestDist=candidateDist;
            }
        }
    }
    nextDistMap[x+width*y]=smallestDist;
}


//An internal distance map only records distances from points inside the shape
//to the region outside the shape.  This kernel sets up initialMap prior to
//running jumpflood.  Any point inside the shape gets initial value HIGH VALUE.  Points
//outside the shape obviously get initial value -1 so that points on outside border
//of shape get value 0
__kernel void internalDistanceMapInit(
        global int* shape,
        global float* initialMap
)
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int width = get_global_size(0);
    int idx = x+width*y;
    initialMap[idx] = shape[x+width*y] ? MAXFLOAT : -1;
}

//This could be an internal or external distance map (ie. distance from outside points to border of shape
//or distance of points inside a shape to the outside).  Which it is is irrelevant - we just
//cound on prevDistMap being accurate
//This is a jumpflood algorithm a la http://www.comp.nus.edu.sg/~tants/jfa/i3d06.pdf
//PRECONDITIONS:
//prevDistMap and nextDistMap are same size
//prevDistMap has valid data.
//k>0
__kernel void internalDistanceMapStep(
            global float* prevDistMap,
            global float* nextDistMap,
            int k)
{
    int width = get_global_size(0);
    int height = get_global_size(1);
    int x=get_global_id(0);
    int y=get_global_id(1);

    float smallestDist = prevDistMap[x+width*y];

    for(int i=-1; i<=1; i++)
    {
        for(int j=-1; j<=1; j++)
        {
            if(i==0 && j==0) continue;
            int candidateX=x+k*i;
            int candidateY=y+k*j;
            if(candidateX<0 || candidateY<0 ||
               candidateX>=width || candidateY>=height) continue;
            int candidateIdx = candidateX + candidateY*width;
            float candidateDist = prevDistMap[candidateIdx] +
                                abs(i*k) + abs(j*k);
            if(candidateDist<smallestDist)
            {
                smallestDist=candidateDist;
            }
        }
    }
    nextDistMap[x+width*y]=smallestDist;
}

//readImage is the big one.  writeImage, a smaller one, needs to be filled
//from readImage.
//PRECONDITIONS:
//-It is assumed that the two images have valid sizes and that the work domain is exactly the size of the writeImage.
//-readImage is larger enough than writeImage that no divide by zero error will occur.
__kernel void downsampleRGBImage(
                __read_only image2d_t readImage,
                __write_only image2d_t writeImage
    ) {

        int largeWidth = get_image_width(readImage);
        int largeHeight = get_image_height(readImage);
        int smallWidth=get_global_size(0);
        int smallHeight=get_global_size(1);

        int x=get_global_id(0);
        int y=get_global_id(1);
        const int2 writePos = {x,y};

        //find the topleft corner of the smaller image pixel in the
        //space of the larger (original) image.
        float left = (((float)x)/((float)(smallWidth)))*((float)(largeWidth-1));
        float top = (((float)y)/((float)(smallHeight)))*((float)(largeHeight-1));
        float right = (((float)(x+1))/((float)(smallWidth)))*((float)(largeWidth-1));
        float bottom = (((float)(y+1))/((float)(smallHeight)))*((float)(largeHeight-1));

        float4 sum={0,0,0,0};
        float numEncountered=0;

        for(int xOld = (int)ceil(left); xOld<(int)ceil(right); xOld++)
        {
            for(int yOld = (int)ceil(top); yOld<(int)ceil(bottom); yOld++)
            {
                int2 readPos = {xOld,yOld};
                sum += read_imagef(readImage,sampler,readPos);
                numEncountered+=1.0;
            }
        }

        write_imagef(writeImage,writePos,sum/numEncountered);
}

//PRECONDITIONS:
//-It is assumed that the two images have valid sizes and that the work domain is exactly the size of the writeImage.
//-readImage is larger enough than writeImage that each of writeImage's pixels map to at least one of its
//-readImageWidth and readImageHeight correspond to actual width and height of readImage
__kernel void downsampleBooleanImage(
                __global int* readImage,
                __global int* writeImage,
                int readImageWidth,
                int readImageHeight,
                int truesPrevail //this is treated as a boolean
    ) {

        int smallWidth=get_global_size(0);
        int smallHeight=get_global_size(1);

        int x=get_global_id(0);
        int y=get_global_id(1);

        //find the topleft corner of the smaller image pixel in the
        //space of the larger (original) image.
        float left = (((float)x)/((float)(smallWidth)))*((float)(readImageWidth-1));
        float top = (((float)y)/((float)(smallHeight)))*((float)(readImageHeight-1));
        float right = (((float)(x+1))/((float)(smallWidth)))*((float)(readImageWidth-1));
        float bottom = (((float)(y+1))/((float)(smallHeight)))*((float)(readImageHeight-1));

        int result= truesPrevail ? 0 : 1; //! op

        for(int xOld = (int)ceil(left); xOld<(int)ceil(right); xOld++)
        {
            if(result==(bool)truesPrevail) break;
            for(int yOld = (int)ceil(top); yOld<(int)ceil(bottom); yOld++)
            {
                int oldValue = readImage[xOld + yOld*readImageWidth];
                if(oldValue==truesPrevail)
                {
                    result=truesPrevail;
                    break;
                }
            }
        }

        writeImage[x+smallWidth*y] = result;
}
