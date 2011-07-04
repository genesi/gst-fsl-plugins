/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/ 

/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */ 



/*
 * Module Name:    aiurregistry.h
 *
 * Description:    Head file of unified parser core functions
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#ifndef __AIURREGISTRY_H__
#define __AIURREGISTRY_H__




typedef struct
{
    /* creation & deletion */
    FslParserVersionInfo getVersionInfo;
    FslCreateParser createParser;
    FslDeleteParser deleteParser;

    /* index export/import */
    FslParserInitializeIndex initializeIndex;
    FslParserImportIndex importIndex;
    FslParserExportIndex exportIndex;

    /* movie properties */
    FslParserIsSeekable isSeekable;
    FslParserGetMovieDuration getMovieDuration;
    FslParserGetUserData getUserData;
    FslParserGetMetaData getMetaData;

    FslParserGetNumTracks getNumTracks;

    FslParserGetNumPrograms getNumPrograms;
    FslParserGetProgramTracks getProgramTracks;

    /* generic track properties */
    FslParserGetTrackType getTrackType;
    FslParserGetDecSpecificInfo getDecoderSpecificInfo;
    FslParserGetTrackDuration getTrackDuration;
    FslParserGetLanguage getLanguage;
    FslParserGetBitRate getBitRate;

    /* video properties */
    FslParserGetVideoFrameWidth getVideoFrameWidth;
    FslParserGetVideoFrameHeight getVideoFrameHeight;
    FslParserGetVideoFrameRate getVideoFrameRate;

    /* audio properties */
    FslParserGetAudioNumChannels getAudioNumChannels;
    FslParserGetAudioSampleRate getAudioSampleRate;
    FslParserGetAudioBitsPerSample getAudioBitsPerSample;
    FslParserGetAudioBlockAlign getAudioBlockAlign;
    FslParserGetAudioChannelMask getAudioChannelMask;
    FslParserGetAudioBitsPerFrame getAudioBitsPerFrame;

    /* text/subtitle properties */
    FslParserGetTextTrackWidth getTextTrackWidth;
    FslParserGetTextTrackHeight getTextTrackHeight;

    /* sample reading, seek & trick mode */
    FslParserGetReadMode getReadMode;
    FslParserSetReadMode setReadMode;

    FslParserEnableTrack enableTrack;

    FslParserGetNextSample getNextSample;
    FslParserGetNextSyncSample getNextSyncSample;

    FslParserGetFileNextSample getFileNextSample;
    FslParserGetFileNextSyncSample getFileNextSyncSample;

    FslParserSeek seek;

    /* add new interface here */

    void *dl_handle;            /* must be last, for dl handle */

} AiurCoreInterface;



#endif /* __AIURREGISTRY_H__ */
