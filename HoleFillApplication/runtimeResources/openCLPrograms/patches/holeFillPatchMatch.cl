/*
Functionality specific to hole filling patch match
*/

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

//This method of an LCG random sequence originally comes
//from Java.util.random.next()'s documentation.  Adaptation to OpenCL
//was demonstrated at stackoverflow.com/questions/9912143/how-to-get-a-random-number-in-opencl
int nextRand(global ulong* seeds, int index)
{
    ulong seed = seeds[index];
    seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1);
    seeds[index] = seed;
    uint unsignedResult = (seed >> 16);
    int result = unsignedResult;
    return abs(result);
}

bool isValidAnchorPosition(int2 coord, int2 dims, int patchWidth)
{
    if(coord.x<patchWidth/2 || coord.x>=dims.x-patchWidth/2 ||
       coord.y<patchWidth/2 || coord.y>=dims.y-patchWidth/2)
       {
        return false;
       }
    return true;
}

float patchCost(
    int2 sourceCoord,
    int2 targetCoord,
    int patchWidth,
    __read_only image2d_t target,
    __read_only image2d_t source,
    global float* anchorWeights,
    float costNotToExceed)
{
    float sumCost=0;
    int targetWidth = get_image_width(target);
    int targetX = targetCoord.x;
    int targetY = targetCoord.y;
    int sourceX = sourceCoord.x;
    int sourceY = sourceCoord.y;

    //note that this loop structure is built around two assumptions:
    // -Patch origin is at center
    // -Patch width is odd.
    for(int patchX = -patchWidth/2; patchX<=patchWidth/2; patchX++)
    {
        for(int patchY=-patchWidth/2; patchY<=patchWidth/2; patchY++)
        {
            int2 targetCoord = {targetX + patchX, targetY + patchY};
            int2 sourceCoord = {sourceX + patchX, sourceY + patchY};

            float4 sourceColor = read_imagef(source,sampler,sourceCoord);
            float4 targetColor = read_imagef(target,sampler,targetCoord);

            //add rgb sum squared difference
            float rDiff=sourceColor.x-targetColor.x;
            float gDiff=sourceColor.y-targetColor.y;
            float bDiff=sourceColor.z-targetColor.z;

            //some target pixels are more imperative to match than others - the weights
            //image gives us an indication of which target pixels are more costly than others.
            float contribution = rDiff*rDiff + gDiff*gDiff + bDiff*bDiff;

            //MODFLAG:  Modifying patch cost by anchor weight.
            contribution*=anchorWeights[targetX + targetY*targetWidth];

            sumCost+=contribution;
            if(sumCost>costNotToExceed)
            {
                return sumCost;
            }
        }
    }
    return sumCost;
}

//PRECONDITIONS:
//distMap and anchorWeights are same size.
#define HF_OUTSIDEHOLEWEIGHT 1000
#define HF_GAMMA 2.0
#define HF_OVERLAPBOOST 2.0
#define HF_SMALLESTWEIGHTALLOWED 0.00000001
__kernel void anchorWeightsFromInternalDistMap(
                global float* distMap, //read
                global float* anchorWeights, //write
                int patchWidth
            )
{
    float overlapDist = ((float)patchWidth)*0.5;

    int x=get_global_id(0);
    int y=get_global_id(1);
    int width = get_global_size(0);
    int idx = x+width*y;

    float dist = distMap[idx];

    if(dist<0)
    {
        //this is outside the hole.  We are still assigning it
        //a weight because we want to weight outside the hole
        //participants in the patch cost step.
        anchorWeights[idx] = HF_OUTSIDEHOLEWEIGHT;
    }
    else
    {
        float weight = pow( (float)HF_GAMMA, (float)( 1-dist ) );
        if(weight<=overlapDist)
        {
            weight*=HF_OVERLAPBOOST;
        }
        if(weight<HF_SMALLESTWEIGHTALLOWED)
        {
            weight=HF_SMALLESTWEIGHTALLOWED;
        }
        anchorWeights[idx] = weight;
    }
}

//Very hole-filling specific.  Source mask needs to be complementOf(target
//PRECONDITIONS:
//-targetMask and sourceMask are same size
//-patchWidth is valid (odd number>1 and less than dims of targetMask/sourceMask)
__kernel void sourceMaskFromTargetMask(
        global int* targetMask,
        global int* sourceMask,
        int patchWidth
)
{
    int posX = get_global_id(0);
    int posY = get_global_id(1);
    int width = get_global_size(0);
    int height = get_global_size(1);

    int result=1;
    for(int x = posX-patchWidth/2; x<=posX+patchWidth/2; x++)
    {
        for(int y = posY-patchWidth/2; y<=posY+patchWidth/2; y++)
        {
            if(x<0 || y<0 || x>width-1 || y>height-1) continue;
            if(targetMask[x+y*width]!=0)
            {
                result=0;
                break;
            }
        }
        if(result==0) break;
    }
    sourceMask[posX+width*posY]=result;
}

/*
// for diagnosing problems with components - DEBUG code
__kernel void blend(
        global int* nnfCoords, //read only
        global int* targetMask, //read only
        global int* sourceMask, //read only
        global float* anchorWeights, //read only
        __read_only image2d_t sourceImagePyramidSize,
        __write_only image2d_t targetImagePyramidSize,
        int patchWidth
)
{

        int x=get_global_id(0);
        int y=get_global_id(1);
        int targetWidth = get_global_size(0);
        const int2 writePos = {x,y};
        int idx = x+y*targetWidth;

        float4 writeVal = {0,1,0,1};

        int targetMaskVal = targetMask[idx];
        int sourceMaskVal = sourceMask[idx];

        float anchorWeight = anchorWeights[idx];

        if(anchorWeight>HF_OUTSIDEHOLEWEIGHT-1)
        {
            writeVal = (float4)(1,0,0,1.0);
        }
        else
        {
            float val = anchorWeight/(HF_OVERLAPBOOST*HF_GAMMA);
            if(val>1.0) val=1.0;
            if(anchorWeight==HF_SMALLESTWEIGHTALLOWED)
            {
                writeVal = (float4)(0,0,1.0,1.0);
            }
            else
            {
                writeVal = (float4)(val,val,val,1.0);
            }
        }

        write_imagef(targetImagePyramidSize,writePos,writeVal);
}*/


__kernel void blend(
                global int* nnfCoords, //read only
                global int* targetMask, //read only
                global int* sourceMask, //read only
                global float* anchorWeights, //read only
                __read_only image2d_t sourceImagePyramidSize,
                __write_only image2d_t targetImagePyramidSize,
                int patchWidth
    )
{

    int x=get_global_id(0);
    int y=get_global_id(1);
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);
    int2 targetDims = {targetWidth,targetHeight};
    int sourceWidth = get_image_width(sourceImagePyramidSize);
    int sourceHeight = get_image_height(sourceImagePyramidSize);
    int2 targetPos = {x,y};
    int targetIdx = x+y*targetWidth;

    //if this is masked out, make no change (leave target as is).  This is assuming
    //that when we reached the current pyramid level, targetImagePyramidSize was
    //downsampled from targetOriginalSize and the masked pixels in it have _not_ been touched
    //since.
    if(!targetMask[targetIdx])
    {
        return;
    }

    bool noValidContributors=true;
    float4 sum = {0,0,0,0};
    float weightSum=0.0;
    for(int patchX = -patchWidth/2; patchX<=patchWidth/2; patchX++)
    {
        for(int patchY=-patchWidth/2; patchY<=patchWidth/2; patchY++)
        {
            int targetAnchorX = x + patchX;
            int targetAnchorY = y + patchY;
            int2 targetAnchor = {targetAnchorX,targetAnchorY};
            if(!isValidAnchorPosition(targetAnchor,targetDims,patchWidth))
            {
                continue;
            }

            //neighbor might not be active anchor, even if it is a valid target
            //anchor position
            if(!targetMask[targetAnchorX+targetWidth*targetAnchorY]) continue;

            //neighbor might point to a masked-out source anchor.  This is inevitable when
            //we get into multiscale pyramids
            int sourceAnchorX = nnfCoords[2*(targetAnchorX+targetWidth*targetAnchorY)];
            int sourceAnchorY = nnfCoords[2*(targetAnchorX+targetWidth*targetAnchorY)+1];

            int sourceCoordX = sourceAnchorX - patchX;
            int sourceCoordY = sourceAnchorY - patchY;
            if(!sourceMask[sourceCoordX + sourceCoordY*sourceWidth]) continue;

            float4 color = read_imagef(sourceImagePyramidSize,sampler,(int2)(sourceCoordX,sourceCoordY));

            //MODFLAG: coherence
            float coherenceAmount=0;
            for(int i=-1; i<=1; i++)
            {
                for(int j=-1; j<=1; j++)
                {
                    if(i==0 && j==0) continue;
                    int otherSourceAnchorX = nnfCoords[2*(targetAnchorX+i+(targetAnchorY+i)*targetWidth)];
                    int otherSourceAnchorY = nnfCoords[2*(targetAnchorX+i+(targetAnchorY+i)*targetWidth)+1];
                    if(otherSourceAnchorX == sourceAnchorX + i &&
                       otherSourceAnchorY == sourceAnchorY + j)
                    {
                        coherenceAmount+=1.0;
                    }
                }
            }

            float weight=anchorWeights[targetAnchorX + targetAnchorY*targetWidth]
                                    + coherenceAmount*coherenceAmount*0.5;

            sum += color*weight;
            weightSum+=weight;
            noValidContributors=false;
        }
    }

    //Our weight sum can be zero in two cases:
    //  -bad weights are assigned to anchorWeights (ie. zeroes or negative numbers)
    //  -our blend pixel is an an invalid anchor position (on the border of the image) and has
    //   no valid-anchor un-masked contributing neighbors.  In short, noValidContributors=true.
    //
    if(noValidContributors)
    {
        //Set to a warning color, but do _not_ just do nothing; that
        //can result in the Bryce canyon bug, where the original hole material started
        //to appear inside the hole, even though that should be technically possible (note
        //that this only occurred when patchCost was temporarily hacked to always return 0.0).
        //WARNIN':  This is a flaw in my PatchMatch algorithm - I do not currently know what
        //to do other than output an obvious warning color.
        write_imagef(targetImagePyramidSize,targetPos,(float4)(0,1,0,1));
    }
    else
    {
        write_imagef(targetImagePyramidSize,targetPos,sum/weightSum);
    }

}

//done immediately prior to initialHoleFill
__kernel void blackOutMaskedArea(
        global int* targetMask, //read only
        __write_only image2d_t targetImage)
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int targetWidth = get_global_size(0);
    if(targetMask[x+targetWidth*y])
    {
        write_imagef(targetImage,(int2)(x,y),(float4)(0,0,0,1));
    }
}

__kernel void initialHoleFill(
        global int* targetMask, //read only
        __read_only image2d_t readImage,
        __write_only image2d_t writeImage)
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);

    if(!targetMask[x+targetWidth*y]) return;

    float4 sum = {0,0,0,0};
    float weightSum=0;
    for(int i=-1; i<=1; i++)
    {
        for(int j=-1; j<=1; j++)
        {
            int srcX = x+i;
            int srcY = y+j;
            if(srcX<0 || srcY<0 || srcX>targetWidth-1 || srcY>targetHeight-1) continue;
            sum += read_imagef(readImage,sampler,(int2)(srcX,srcY));
            weightSum+=1.0;
        }
    }
    sum/=weightSum;
    write_imagef(writeImage,(int2)(x,y),sum);
}


//This is jumpflood - that's what k is about.
__kernel void propagate(
            global float* anchorWeights,
            __read_only image2d_t targetImage,
            __read_only image2d_t sourceImage,
            global int* targetMask,
            global int* sourceMask,
            int patchWidth,
            int k,
            global int* nnfCoordsRead, //readonly.
            global int* nnfCoordsWrite, //writeonly
            global float* nnfCostsRead, //readonly
            global float* nnfCostsWrite //writeonly
        )
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);
    int targetIdx = x + targetWidth*y;
    int2 targetCoord = {x,y};
    int2 targetDims = {targetWidth, targetHeight };
    int sourceWidth = get_image_width(sourceImage);
    int sourceHeight = get_image_height(sourceImage);
    int2 sourceDims = {sourceWidth,sourceHeight};

    float bestMatchCost = nnfCostsRead[targetIdx];
    int bestMatchCoordX = nnfCoordsRead[2*targetIdx];
    int bestMatchCoordY = nnfCoordsRead[2*targetIdx+1];

    //check every nnf neighbor in {x+i,y+j}, where i and j are {-k,0,k}.
    //See if their propositions are better
    for(int i=-k; i<=k; i+=k)
    {
        for(int j=-k; j<=k; j++)
        {
            if(i==0 && j==0) continue;
            int votingNeighborX = x+i;
            int votingNeighborY = y+j;

            if(!isValidAnchorPosition((int2)(votingNeighborX,votingNeighborY),
                                       targetDims,patchWidth))
            {
                continue;
            }
            if(!targetMask[votingNeighborX + votingNeighborY*targetWidth]) continue;
            int candidateMatchX = nnfCoordsRead[2*(votingNeighborX + votingNeighborY*targetWidth)] - i;
            int candidateMatchY = nnfCoordsRead[2*(votingNeighborX + votingNeighborY*targetWidth)+1] - j;
            if(!isValidAnchorPosition((int2)(candidateMatchX,candidateMatchY),sourceDims,patchWidth))
            {
                continue;
            }
            if(!sourceMask[candidateMatchX+sourceWidth*candidateMatchY])
            {
                continue;
            }


            float matchCost = patchCost((int2)(candidateMatchX,candidateMatchY),
                    targetCoord,patchWidth,targetImage,sourceImage,
                    anchorWeights,bestMatchCost);
            if(matchCost<bestMatchCost)
            {
                bestMatchCost=matchCost;
                bestMatchCoordX = candidateMatchX;
                bestMatchCoordY = candidateMatchY;
            }
        }
    }

    nnfCoordsWrite[2*targetIdx] = bestMatchCoordX;
    nnfCoordsWrite[2*targetIdx+1] = bestMatchCoordY;
    nnfCostsWrite[targetIdx] = bestMatchCost;

}

#define HF_SEARCH_ALPHA 0.5
__kernel void search(
        global ulong* randomSeeds,
        global float* anchorWeights,
        __read_only image2d_t targetImage,
        __read_only image2d_t sourceImage,
        global int* targetMask,
        global int* sourceMask,
        int patchWidth,
        global int* nnfCoords, //readandwrite
        global float* nnfCosts //readandwrite
        )
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int2 targetCoord = {x,y};
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);
    int2 targetDims = { targetWidth,targetHeight };
    int sourceWidth = get_image_width(sourceImage);
    int sourceHeight = get_image_height(sourceImage);
    int2 sourceDims = {sourceWidth,sourceHeight};
    int targetIndex = x+targetWidth*y;


    //obviously do not search for target anchors that are invalid positions
    //or that are masked out
    if(!isValidAnchorPosition(targetCoord,targetDims,patchWidth) ||
       !targetMask[targetIndex])
    {
        return;
    }

    int sourceAnchorX = nnfCoords[2*targetIndex];
    int sourceAnchorY = nnfCoords[2*targetIndex+1];
    float currentCost = nnfCosts[targetIndex];
    float searchRadius = max(sourceWidth,sourceHeight);
    while(searchRadius>1)
    {
        int minX = (int)((float)sourceAnchorX-searchRadius);
        if(minX<patchWidth/2) minX=patchWidth/2;
        int maxX = (int)((float)sourceAnchorX+searchRadius);
        if(maxX>sourceWidth-patchWidth/2-1) maxX=sourceWidth-patchWidth/2-1;

        int minY = (int)((float)sourceAnchorY-searchRadius);
        if(minY<patchWidth/2) minY=patchWidth/2;
        int maxY = (int)((float)sourceAnchorY+searchRadius);
        if(maxY>sourceHeight-patchWidth/2-1) maxY=sourceHeight-patchWidth/2-1;

        int candidateSourceX = ((nextRand(randomSeeds,targetIndex))%(maxX-minX+1)) + minX;
        int candidateSourceY = ((nextRand(randomSeeds,targetIndex))%(maxY-minY+1)) + minY;

        //ignore candidates which are part of forbidden source
        if(sourceMask[candidateSourceX+sourceWidth*candidateSourceY])
        {
            float potentialMatchCost = patchCost((int2)(candidateSourceX,candidateSourceY),
                                                targetCoord,patchWidth,targetImage,sourceImage,
                                                anchorWeights,currentCost);
            if(potentialMatchCost<currentCost)
            {
                sourceAnchorX = candidateSourceX;
                sourceAnchorY = candidateSourceY;
                currentCost = potentialMatchCost;
            }
        }

        searchRadius*=HF_SEARCH_ALPHA;
    }

    //record results
    nnfCoords[2*targetIndex] = sourceAnchorX;
    nnfCoords[2*targetIndex+1] = sourceAnchorY;
    nnfCosts[targetIndex] = currentCost;

}

//This differs markedly from the CPU implementation.  There is no more per-pixel loop
//to search for unmasked valid source coord when necessary.  Instead, if an upsampled
//source coord is an invalid anchor pos, we assign a random valid pos, and do not even
//worry about it being masked.
__kernel void nnfUpsampleCoords(
        int prevTargetWidth,
        int prevTargetHeight,
        global int* nextTargetMask,
        global int* nextSourceMask,
        int nextSourceWidth,
        int nextSourceHeight,
        int prevSourceWidth,
        int prevSourceHeight,
        int patchWidth,
        global ulong* randomBuffer,
        global int* prevNNFCoords,
        global int* nextNNFCoords)
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int2 targetCoord = {x,y};
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);
    int2 targetDims = {targetWidth,targetHeight};
    int2 sourceDims = {nextSourceWidth,nextSourceHeight};
    int2 prevTargetDims = {prevTargetWidth,prevTargetHeight};
    int targetIndex = x+targetWidth*y;

    //is (newX,newY) an invalid anchor position or masked?  If so,
    //just fill with a bogus coordinate and jettison.
    if(!isValidAnchorPosition(targetCoord,targetDims,patchWidth) ||
       !nextTargetMask[targetIndex])
    {
        nextNNFCoords[2*targetIndex] = 0;
        nextNNFCoords[2*targetIndex+1] = 0;
        return;
    }

    //We take _nnf's previous source coord for (x,y) converted to
    // (oldX,oldY), convert that source coord to new space, and set new _nnf to this for
    //(x,y).  There are a few things to keep in mind:
    // - (oldX,oldY) may be invalid anchor position for previous size of _targetPyramidSizeRgb
    // - (oldX,oldY) may be masked by the previous version of _targetMask
    // - nnf's entry for (oldX,oldY) maps to a source coord which, when converted to new source coord space,
    //   is an invalid anchor position or is masked.
    //  The only thing we _must_ guarantee is that new nnf's coord is a valid source anchor position.

    //convert an integer point in {[0,targetSize.x()-1],[0,targetSize.y()-1]} to a
    //floating point coord in {[0,oldWidth-1],[0,oldHeight-1]} so we can upsample.
    int oldX = (int)((((float)x)/((float)(targetWidth-1)))*((float)(prevTargetWidth-1)));
    int oldY = (int)((((float)y)/((float)(targetHeight-1)))*((float)(prevTargetHeight-1)));
    int2 oldCoord = {oldX,oldY};
    int upsampledX=0, upsampledY=0;

    if(isValidAnchorPosition(oldCoord,prevTargetDims,patchWidth))
    {
        //just do "nearest neighbor" by casting back to int.  Interpolating values does not
        //really work here, because adjacent entries in _nnf may differ greatly
        upsampledX = prevNNFCoords[2*(oldX + oldY*prevTargetWidth)];
        upsampledY = prevNNFCoords[2*(oldX + oldY*prevTargetWidth)+1];

        //need to convert old source coord from previous pyramid level size to new
        //pyramid leve size
        upsampledX = (int)((((float)upsampledX)/((float)(prevSourceWidth-1)))*((float)(nextSourceWidth-1)));
        upsampledY = (int)((((float)upsampledY)/((float)(prevSourceHeight-1)))*((float)(nextSourceHeight-1)));


    }

    if(!isValidAnchorPosition((int2)(upsampledX,upsampledY),sourceDims,patchWidth))
    {
        //assign some random, valid source coordinate
        upsampledX = patchWidth/2 + nextRand(randomBuffer,targetIndex)%(nextSourceWidth-patchWidth);
        upsampledY = patchWidth/2 + nextRand(randomBuffer,targetIndex)%(nextSourceHeight-patchWidth);
    }

    nextNNFCoords[2*targetIndex] = upsampledX;
    nextNNFCoords[2*targetIndex+1] = upsampledY;


}

//meant to be called right after nnfUpsample, which fills nnfCoords.
__kernel void nnfCosts(
            __read_only image2d_t targetImage,
            __read_only image2d_t sourceImage,
            global float* anchorWeights,
            global int* targetMask,
            global int* sourceMask,
            int sourceWidth,
            int patchWidth,
            global int* nnfCoords, //read only
            global float* nnfCosts //write only
             )
{
    int x=get_global_id(0);
    int y=get_global_id(1);
    int2 targetCoord = {x,y};
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);
    int targetIndex = x+targetWidth*y;

    if(!isValidAnchorPosition(targetCoord,(int2)(targetWidth,targetHeight),patchWidth)
       || !targetMask[targetIndex])
    {
        nnfCosts[targetIndex] = MAXFLOAT;
        return;
    }

    //what source coordinate do I point to?  Note that
    //this value is guaranteed to be a valid source coord if nnfUpsample kernel
    //was done correctly.
    int sourceX = nnfCoords[2*(x + targetWidth*y)];
    int sourceY = nnfCoords[2*(x + targetWidth*y)+1];
    int2 sourceCoord = { sourceX, sourceY };

    //if it's a masked coord, then indicate maximum cost
    if(!sourceMask[sourceX + sourceWidth*sourceY])
    {
        nnfCosts[targetIndex] = MAXFLOAT;
        return;
    }

    float costThere = patchCost(sourceCoord,targetCoord,patchWidth,
                                targetImage,sourceImage,anchorWeights,MAXFLOAT);
    nnfCosts[targetIndex] = costThere;

}

//This is called only for first pyramid level.  We guarantee that
//every coord in nnfCoords for which targetMask!=0 gets mapped to a
//VALID source coord, not necessarily a source coord
//for which sourceMask!=0.  We attempt to ensure the latter, but obviously
//this is impossible to absolutely guarantee.
//Note that costs get filled too
__kernel void nnfInitialFill(
            global int* targetMask, //read only
            global int* sourceMask, //read only
            int sourceWidth,
            int sourceHeight,
            int patchWidth,
            global ulong* randomBuffer,
            __read_only image2d_t targetImage,
            __read_only image2d_t sourceImage,
            global float* anchorWeights,
            global int* nnfCoords,  //write only
            global float* nnfCosts) //write only
{

    int x=get_global_id(0);
    int y=get_global_id(1);
    int2 targetCoord = {x,y};
    int targetWidth = get_global_size(0);
    int targetHeight = get_global_size(1);
    int targetIndex = x+targetWidth*y;

    //skip invalid target anchors and masked pixels
    if(!isValidAnchorPosition((int2)(x,y),(int2)(targetWidth,targetHeight),patchWidth) ||
        !targetMask[targetIndex])
    {
        //still assign at least a bogus value in anticipation
        //of future upsampling from this nnf buffer
        nnfCoords[2*targetIndex] = 0;
        nnfCoords[2*targetIndex+1] = 0;
        nnfCosts[targetIndex] = MAXFLOAT;
        return;
    }

    int maxTriesPerPixel = 10*max(targetWidth,targetHeight);

    for(int attempt=0; attempt<maxTriesPerPixel; attempt++)
    {
        int sourceX = patchWidth/2 + nextRand(randomBuffer,targetIndex)%(sourceWidth-patchWidth);
        int sourceY = patchWidth/2 + nextRand(randomBuffer,targetIndex)%(sourceHeight-patchWidth);

        nnfCoords[2*targetIndex] = sourceX;
        nnfCoords[2*targetIndex+1] = sourceY;
        if(sourceMask[sourceX + sourceY*sourceWidth])
        {
            //we have found a source coord that is valid _and_ unmasked.  We are done - just
            //determine the cost of this match
            float costThere = patchCost((int2)(sourceX,sourceY),
                                        targetCoord,patchWidth,
                                 targetImage,sourceImage,
                                 anchorWeights,MAXFLOAT);

            nnfCosts[targetIndex] = costThere;
            break;
        }
        else
        {
            //this source coord is a valid position but masked.  We will put it in the _nnf for (x,y) for now,
            //but keep trying to find a source coord that is _not_ masked
            nnfCosts[targetIndex] = MAXFLOAT;
        }
    }
}


