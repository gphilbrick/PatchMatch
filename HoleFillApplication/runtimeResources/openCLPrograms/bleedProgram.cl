__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel void testKernel(
		__read_only image2d_t readImage,
        __write_only image2d_t writeImage
    ) {

	int imageWidth=get_global_size(0);
	int imageHeight=get_global_size(1);
	
    const int2 pos = {get_global_id(0), get_global_id(1)};	
	int x=pos.x;
	int y=pos.y;
	
	float4 myValue = read_imagef(readImage,sampler,pos);
	float4 mostDifferentValue=myValue;
	float mostDifference=0;
	
	//consider my neighbors
	for(int i=-1; i<=1; i++)
	{
		for(int j=-1; j<=1; j++)
		{
			if(i==0 && j==0) continue;
			int neighborX = x+i;
			int neighborY = y+j;			
			//forbid illegal lookups 
			if(neighborX<0 || neighborY<0 || neighborX>imageWidth-1 ||
			   neighborY>imageHeight-1) continue;
			   
			int2 neighborPos = {neighborX, neighborY};
			float4 neighborValue = read_imagef(readImage,sampler,neighborPos);
			float rDiff = myValue.x - neighborValue.x;
			float gDiff = myValue.y - neighborValue.y;
			float bDiff = myValue.z - neighborValue.z;
			float diff = rDiff*rDiff + gDiff*gDiff + bDiff*bDiff;
			if(diff > mostDifference)
			{
				mostDifference = diff;
				mostDifferentValue = neighborValue;
			}
		}
	}

    write_imagef(writeImage,pos,mostDifferentValue);
}
