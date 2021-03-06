/*++

Module Name:

    WGSim.cpp

Abstract:

    Handle FASTQ ID strings like the ones generated by WGSim

Authors:

    Bill Bolosky, August, 2011

Environment:

    User mode service.

Revision History:

    Pulled from cSNAP to make it useful in more than one place

--*/

#include "stdafx.h"
#include "WGsim.h"
#include "exit.h"

using namespace std;

// Is a wgsim-generated read mapped to a given location misaligned, given the source
// location encoded into its ID and a maximum edit distance maxK?
// Also optionally outputs the low and high location encoded in the wgsim read's ID.
bool wgsimReadMisaligned(Read *read, unsigned location, GenomeIndex *index, int maxK,
                         unsigned *lowOut, unsigned *highOut)
{
    return wgsimReadMisaligned(read, location, index->getGenome(), maxK, lowOut, highOut);
}

bool wgsimReadMisaligned(Read *read, unsigned genomeLocation, const Genome *genome, int maxK,
                         unsigned *lowOut, unsigned *highOut)
{
    //
    // The read ID for wgsim-generated reads is of the format:
    //
    //     contig_begin_end_:otherStuff
    //
    // In order to make it more complicated, "contig" may contain "_", and
    // "otherStuff" may contain ":".  So we handle it by searching for the
    // first ":", then going back two "_" to find the offsets, and finally
    // deciding that everything after the before the "_" that we just found
    // must be the contig name.  Sigh.
    //
    unsigned offset1, offset2;
    char id[1024];
    if (read->getIdLength() > sizeof(id) - 1) {
      fprintf(stderr, "Got a read ID that was too long! It starts with %s\n", id);
      soft_exit(1);
    }
    unsigned toCopy = min(read->getIdLength(), (unsigned) sizeof(id) - 1);
    strncpy(id, read->getId(), toCopy);
    id[toCopy] = '\0';
    const char *firstColon = strchr(id, ':');
    if (firstColon == NULL) {
        fprintf(stderr, "Failed to parse read id '%s', couldn't find a colon.\n",id);
        return false;
    }

    const char *underscoreBeforeColon;
    for (underscoreBeforeColon = firstColon - 1; 
         underscoreBeforeColon >= id && *underscoreBeforeColon != '_';
         underscoreBeforeColon--) {
        // This loop body intentionally left blank.
    }

    if (underscoreBeforeColon < id) {
        fprintf(stderr,"Failed to parse read id '%s', couldn't find underscore before colon.\n",id);
        return false;
    }

    const char *secondUnderscoreBeforeColon;
    for (secondUnderscoreBeforeColon = underscoreBeforeColon - 1; 
        secondUnderscoreBeforeColon >= id && *secondUnderscoreBeforeColon != '_'; 
        secondUnderscoreBeforeColon--) {
        // This loop body intentionally left blank.
    }

    if (secondUnderscoreBeforeColon < id) {
        fprintf(stderr, "Failed to parse read id '%s', couldn't find second underscore before colon.\n",id);
        return false;
    }

    const char *thirdUnderscoreBeforeColon;
    for (thirdUnderscoreBeforeColon = secondUnderscoreBeforeColon - 1; 
        thirdUnderscoreBeforeColon >= id && *thirdUnderscoreBeforeColon != '_'; 
        thirdUnderscoreBeforeColon--) {
        // This loop body intentionally left blank.
    }

    if (thirdUnderscoreBeforeColon < id) {
        fprintf(stderr,"Failed to parse read id '%s', couldn't find third underscore before colon.\n",id);
        return false;
    }

    if (1 != sscanf(thirdUnderscoreBeforeColon+1, "%d", &offset1)) {
        fprintf(stderr,"Failed to parse read id '%s', couldn't parse offset1.\n",id);
        return false;
    }

    if (underscoreBeforeColon == secondUnderscoreBeforeColon + 1) {
        // No second offset given, since this is a single-end read; just use the first offset
        offset2 = offset1;
    } else {
        if (1 != sscanf(secondUnderscoreBeforeColon+1, "%d", &offset2)) {
            fprintf(stderr,"Failed to parse read id '%s', couldn't parse offset2.\n",id);
            return false;
        }
    }
    
    //
    // Look up the contig to get its offset in the whole genome, and add that in to the offsets.
    // This is because our index treats the entire genome as one big thing, while the FASTQ file
    // treats each contig separately.
    //

    const size_t contigNameMaxSize = 200;
    char contigName[contigNameMaxSize];

    size_t contigNameLen = thirdUnderscoreBeforeColon - id;

    if (contigNameLen >= contigNameMaxSize) {
        fprintf(stderr, "Contig name too big or misparsed, '%s'\n",id);
        return false;
    }

    memcpy(contigName, id, contigNameLen);
    contigName[contigNameLen] = '\0';

    unsigned offsetOfContig;
    if (!genome->getOffsetOfContig(contigName,&offsetOfContig)) {
        fprintf(stderr, "Couldn't find contig name '%s' in the genome.\n",contigName);
        return false;
    }

    offset1 = offset1 + offsetOfContig - 1;  // It's one-based and the Aligner is zero-based
    offset2 = offset2 + offsetOfContig - 1;  // It's one-based and the Aligner is zero-based
    
    unsigned high = max(offset1, offset2);
    unsigned low = min(offset1, offset2);
    if (lowOut != NULL)
        *lowOut = low;
    if (highOut != NULL)
        *highOut = high;
        
    return (genomeLocation > high + maxK || genomeLocation + maxK < low);
}

void wgsimGenerateIDString(const Genome::Contig *contig, unsigned offsetInContig,
                           unsigned readLength, bool firstHalf, char *outputBuffer)
{
    // This is a minimal ID string that works for our reader function
    
    sprintf(outputBuffer, "%s_%d_%d_0::0:0_2:0:a0_0/%d", contig->name, offsetInContig + 1,
            offsetInContig + readLength, firstHalf ? 1 : 2);
}

void wgsimGenerateIDString(const Genome *genome, unsigned genomeLocation,
                           unsigned readLength, bool firstHalf, char *outputBuffer)
{
    const Genome::Contig *contig = genome->getContigAtLocation(genomeLocation);

    wgsimGenerateIDString(contig, genomeLocation - contig->beginningOffset,
                          readLength, firstHalf, outputBuffer);
}
