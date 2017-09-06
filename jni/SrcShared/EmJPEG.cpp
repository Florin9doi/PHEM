/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1999-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmJPEG.h"

#include "EmPixMap.h"			// SetSize, GetRowBytes, etc.
#include "EmStream.h"			// GetLength, GetBytes
#include "Platform.h"			// Platform::DisposeMemory, AllocateMemory

#include "PHEMNativeIF.h"

/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSource constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressSource::EmJPEGDecompressSource (void)
{
	this->next_input_byte	= 0;
	this->bytes_in_buffer	= 0;

	// Fill in fields/function pointers used by JPEG engine

	this->init_source		= &init_source_cb;
	this->fill_input_buffer	= &fill_input_buffer_cb;
	this->skip_input_data	= &skip_input_data_cb;
	this->resync_to_restart	= &jpeg_resync_to_restart;
	this->term_source		= &term_source_cb;

}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSource destructor
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressSource::~EmJPEGDecompressSource (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSource::init_source_cb
 *
 * DESCRIPTION:	Callback function that retrieves the pointer to the
 *				object managing the decompression and then calls the
 *				appropriate method of that object.
 *
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressSource::init_source_cb (j_decompress_ptr cinfo)
{
	((EmJPEGDecompressSource*) cinfo->src)->InitSource (cinfo);
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSource::fill_input_buffer_cb
 *
 * DESCRIPTION:	Callback function that retrieves the pointer to the
 *				object managing the decompression and then calls the
 *				appropriate method of that object.
 *
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

boolean EmJPEGDecompressSource::fill_input_buffer_cb (j_decompress_ptr cinfo)
{
	return ((EmJPEGDecompressSource*) cinfo->src)->FillInputBuffer (cinfo);
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSource::skip_input_data_cb
 *
 * DESCRIPTION:	Callback function that retrieves the pointer to the
 *				object managing the decompression and then calls the
 *				appropriate method of that object.
 *
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *				num_bytes - number of bytes to skip.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressSource::skip_input_data_cb (j_decompress_ptr cinfo, long num_bytes)
{
	((EmJPEGDecompressSource*) cinfo->src)->SkipInputData (cinfo, num_bytes);
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSource::term_source_cb
 *
 * DESCRIPTION:	Callback function that retrieves the pointer to the
 *				object managing the decompression and then calls the
 *				appropriate method of that object.
 *
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressSource::term_source_cb (j_decompress_ptr cinfo)
{
	((EmJPEGDecompressSource*) cinfo->src)->TermSource (cinfo);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressMemSource constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	data - pointer to the memory buffer holding the
 *					compressed JPEG data.
 *				len - number of bytes pointed to by "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressMemSource::EmJPEGDecompressMemSource (const void* data, long len) :
	fData (data),
	fDataLen (len)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressMemSource destructor
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressMemSource::~EmJPEGDecompressMemSource (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressMemSource::InitSource
 *
 * DESCRIPTION:	Initialize source --- called by jpeg_read_header
 *				before any data is actually read.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressMemSource::InitSource (j_decompress_ptr /*cinfo*/)
{
	// Fill in fields/function pointers used by JPEG engine

	this->next_input_byte		= (const JOCTET*) fData;
	this->bytes_in_buffer		= (size_t) fDataLen;
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressMemSource::FillInputBuffer
 *
 * DESCRIPTION:	See fill_input_buffer_cb. Unused in this class.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

boolean EmJPEGDecompressMemSource::FillInputBuffer (j_decompress_ptr /*cinfo*/)
{
	EmAssert (false);

	return TRUE;
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressMemSource::SkipInputData
 *
 * DESCRIPTION:	See skip_input_data_cb.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *				num_bytes - number of bytes to skip.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressMemSource::SkipInputData (j_decompress_ptr /*cinfo*/, long num_bytes)
{
	this->next_input_byte += (size_t) num_bytes;
	this->bytes_in_buffer -= (size_t) num_bytes;
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressMemSource::TermSource
 *
 * DESCRIPTION:	See term_source_cb
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressMemSource::TermSource (j_decompress_ptr /*cinfo*/)
{
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressStreamSource constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	data - pointer to the memory buffer holding the
 *					compressed JPEG data.
 *				len - number of bytes pointed to by "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressStreamSource::EmJPEGDecompressStreamSource (EmStream& s) :
	fStream (s),
	fBuffer (NULL)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressStreamSource destructor
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressStreamSource::~EmJPEGDecompressStreamSource (void)
{
	Platform::DisposeMemory (fBuffer);
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressStreamSource::InitSource
 *
 * DESCRIPTION:	Initialize source --- called by jpeg_read_header
 *				before any data is actually read.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressStreamSource::InitSource (j_decompress_ptr /*cinfo*/)
{
	// Fill in fields/function pointers used by JPEG engine

	this->bytes_in_buffer		= (size_t) fStream.GetLength ();
	this->fBuffer				= Platform::AllocateMemory (this->bytes_in_buffer);
	this->next_input_byte		= (const JOCTET*) fBuffer;

	fStream.GetBytes (fBuffer, this->bytes_in_buffer);
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressStreamSource::FillInputBuffer
 *
 * DESCRIPTION:	See fill_input_buffer_cb. Unused in this class.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

boolean EmJPEGDecompressStreamSource::FillInputBuffer (j_decompress_ptr /*cinfo*/)
{
	EmAssert (false);

	return TRUE;
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressStreamSource::SkipInputData
 *
 * DESCRIPTION:	See skip_input_data_cb.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *				num_bytes - number of bytes to skip.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressStreamSource::SkipInputData (j_decompress_ptr /*cinfo*/, long num_bytes)
{
	this->next_input_byte += (size_t) num_bytes;
	this->bytes_in_buffer -= (size_t) num_bytes;
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressStreamSource::TermSource
 *
 * DESCRIPTION:	See term_source_cb
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressStreamSource::TermSource (j_decompress_ptr /*cinfo*/)
{
}


#pragma mark -


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSink::EmJPEGDecompressSink
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressSink::EmJPEGDecompressSink	(void) :
	fBuffer (NULL),
	fBufferHeight (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressSink::~EmJPEGDecompressSink
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressSink::~EmJPEGDecompressSink	(void)
{
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ErrorExit
 *
 * DESCRIPTION:	Error exit handler: must not return to caller.
 *
 * Applications may override this if they want to get control back after
 * an error.  Typically one would longjmp somewhere instead of exiting.
 * The setjmp buffer can be made a private field within an expanded error
 * handler object.  Note that the info needed to generate an error message
 * is stored in the error object, so you can generate the message now or
 * later, at your convenience.
 * You should make sure that the JPEG object is cleaned up (with jpeg_abort
 * or jpeg_destroy) at some point.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static void ErrorExit (j_common_ptr)
{
	// !!! TBD if we want to handle mal-formed JPEG files.
}


/***********************************************************************
 *
 * FUNCTION:	OutputMessage
 *
 * DESCRIPTION:	Actual output of an error or trace message.
 *
 * Applications may override this method to send JPEG messages somewhere
 * other than stderr.
 *
 * On Windows, printing to stderr is generally completely useless,
 * so we provide optional code to produce an error-dialog popup.
 * Most Windows applications will still prefer to override this routine,
 * but if they don't, it'll do something at least marginally useful.
 *
 * NOTE: to use the library in an environment that doesn't support the
 * C stdio library, you may have to delete the call to fprintf() entirely,
 * not just not use this routine.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static void OutputMessage (j_common_ptr)
{
	// !!! TBD if we want to report messages.
}


/***********************************************************************
 *
 * FUNCTION:	EmitMessage
 *
 * DESCRIPTION:	Decide whether to emit a trace or warning message.
 *
 * msg_level is one of:
 *   -1: recoverable corrupt-data warning, may want to abort.
 *    0: important advisory messages (always display to user).
 *    1: first level of tracing detail.
 *    2,3,...: successively more detailed tracing messages.
 * An application might override this method if it wanted to abort on warnings
 * or change the policy about which messages to display.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static void EmitMessage (j_common_ptr, int)
{
	// !!! TBD if we want to report messages
}


/***********************************************************************
 *
 * FUNCTION:	FormatMessage
 *
 * DESCRIPTION:	Format a message string for the most recent JPEG error
 *				or message.
 *
 * The message is stored into buffer, which should be at least JMSG_LENGTH_MAX
 * characters.  Note that no '\n' character is added to the string.
 * Few applications should need to override this method.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static void
FormatMessage (j_common_ptr, char*)
{
	// !!! TBD if we want to report messages
}


/***********************************************************************
 *
 * FUNCTION:	ResetError
 *
 * DESCRIPTION:	Reset error state variables at start of a new image.
 *
 * This is called during compression startup to reset trace/error
 * processing to default state, without losing any application-specific
 * method pointers.  An application might possibly want to override
 * this method if it has additional error processing state.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static void ResetError (j_common_ptr)
{
	// !!! TBD if we want to reset after errors
}


/***********************************************************************
 *
 * FUNCTION:	InitError
 *
 * DESCRIPTION:	Initialize the jpeg_error_mgr object.  We use our own
 *				routine instead of jpeg_std_error so that we don't pull
 *				in a bunch of JPEG lib routines we don't want.
 *
 * PARAMETERS:	err - pointer to the block used by the JPEG error mgr.
 *
 * RETURNED:	Pointer to same.
 *
 ***********************************************************************/

static struct jpeg_error_mgr*
InitError (struct jpeg_error_mgr* err)
{
	err->error_exit = ErrorExit;
	err->emit_message = EmitMessage;
	err->output_message = OutputMessage;
	err->format_message = FormatMessage;
	err->reset_error_mgr = ResetError;

	err->trace_level = 0;		/* default = no tracing */
	err->num_warnings = 0;		/* no warnings emitted yet */
	err->msg_code = 0;			/* may be useful as a flag for "no error" */

	/* Initialize message table pointers */
	err->jpeg_message_table = NULL;
	err->last_jpeg_message = 0;

	err->addon_message_table = NULL;
	err->first_addon_message = 0;	/* for safety */
	err->last_addon_message = 0;

	return err;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressPixMapSink c'tor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressPixMapSink::EmJPEGDecompressPixMapSink (EmPixMap& pixMap) :
	EmJPEGDecompressSink (),
	fPixMap (pixMap),
	fDataWidth (0),
	fRowWidth (0),
	fPadBytes (0),
	fCurOutputRow (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressPixMapSink d'tor
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmJPEGDecompressPixMapSink::~EmJPEGDecompressPixMapSink (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressPixMapSink::post_jpeg_read_header
 *
 * DESCRIPTION:	Called by the JPEG decompression engine after the JPEG
 *				header has been read.  Initializes some of our data
 *				members according to what was read.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void
EmJPEGDecompressPixMapSink::post_jpeg_read_header (j_decompress_ptr cinfo)
{
	// Calculate output image dimensions so we can allocate space

	jpeg_calc_output_dimensions (cinfo);

	// Prepare the EmPixMap.

	EmAssert (cinfo->out_color_space == JCS_RGB);

        PHEM_Log_Msg("EmJPEGDecompressPixMapSink");
        PHEM_Log_Place(cinfo->output_width);
        PHEM_Log_Place(cinfo->output_height);

	fPixMap.SetSize (EmPoint (cinfo->output_width, cinfo->output_height));
	fPixMap.SetFormat (kPixMapFormat24RGB);

	// Determine width of rows in the EmPixMap.

	fDataWidth		= cinfo->output_width * cinfo->output_components;
        PHEM_Log_Msg("fDataWidth:");
        PHEM_Log_Place(fDataWidth);
	fRowWidth		= fPixMap.GetRowBytes ();
        PHEM_Log_Msg("fRowWidth:");
        PHEM_Log_Place(fRowWidth);
	fPadBytes		= fRowWidth - fDataWidth;

        PHEM_Log_Msg("fPadBytes:");
        PHEM_Log_Place(fPadBytes);
	// Prepare for write pass

	fCurOutputRow	= 0;

	// Create decompressor output buffer.  This buffer is passed to
	// jpeg_read_scanlines as a scanline output buffer.

	fBuffer = (*cinfo->mem->alloc_sarray)
		((j_common_ptr) cinfo, JPOOL_IMAGE, fDataWidth, 1);
	fBufferHeight = 1;
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressPixMapSink::start_output
 *
 * DESCRIPTION:	Write any initial part of the output file.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressPixMapSink::start_output (j_decompress_ptr)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressPixMapSink::put_pixel_rows
 *
 * DESCRIPTION:	Write some pixel data.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *				JDIMENSION - number of rows.  This parameter is really
 *					just a holdover from the JPEG sample code; in our
 *					implementation, it's always 1.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressPixMapSink::put_pixel_rows (j_decompress_ptr, JDIMENSION)
{
	int			line	= fCurOutputRow++;
	char*		outptr	= (char*) fPixMap.GetBits () + line * fRowWidth;
	JSAMPROW	inptr	= fBuffer[0];

	// Copy the source pixels.  They're in RGB format, which is what
	// we've set our destination pixmap to be.

	memcpy (outptr, inptr, fDataWidth);

	// Zero out the pad bytes.

	if (fPadBytes)
	{
		memset (outptr + fDataWidth, 0, fPadBytes);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmJPEGDecompressPixMapSink::finish_output
 *
 * DESCRIPTION:	Write any data that needs to be written after all the
 *				pixel rows have been processed.
 *
 * PARAMETERS:	cinfo - data block describing the decompression process.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmJPEGDecompressPixMapSink::finish_output (j_decompress_ptr /* cinfo */)
{
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ConvertJPEG
 *
 * DESCRIPTION:	Drive the entire JPEG decompression process.  Compressed
 *				data is provided by src_mgr; decompressed data is
 *				handled by dest_mgr.  When this function is completed,
 *				the decompresed image can be picked up from dest_mgr.
 *
 * PARAMETERS:	src_mgr - object that provides compressed JPEG data.
 *				dest_mgr - object that handles decompressed JPEG data.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void ConvertJPEG (EmJPEGDecompressSource& src_mgr, EmJPEGDecompressSink& dest_mgr)
{
	// Modelled after main() in djpeg.c.

	struct jpeg_decompress_struct	cinfo;	// Defined by JPEG lib; contains decompression state (including pointer to src manager).
	struct jpeg_error_mgr			jerr;	// Defined by JPEG lib; contains methods for reporting errors


	// Initialize the JPEG decompression object with default error handling.

	cinfo.err = ::InitError (&jerr);
	jpeg_create_decompress (&cinfo);

	// Specify data source for decompression

	cinfo.src = (jpeg_source_mgr*) &src_mgr;

	// Read file header, set default decompression parameters

	(void) jpeg_read_header (&cinfo, TRUE);

	// Initialize the output module now to let it override any crucial
	// option settings (for instance, GIF wants to force color quantization).

	dest_mgr.post_jpeg_read_header (&cinfo);

	// Start decompressor

	(void) jpeg_start_decompress (&cinfo);

	// Write output file header

	dest_mgr.start_output (&cinfo);

	// Process data

	while (cinfo.output_scanline < cinfo.output_height)
	{
		JDIMENSION		num_scanlines;
		num_scanlines = jpeg_read_scanlines (&cinfo,
											dest_mgr.fBuffer,
											dest_mgr.fBufferHeight);
		dest_mgr.put_pixel_rows (&cinfo, num_scanlines);
	}

	// Finish decompression and release memory.
	// I must do it in this order because output module has allocated memory
	// of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.

	dest_mgr.finish_output (&cinfo);
	(void) jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);
}


/***********************************************************************
 *
 * FUNCTION:	JPEGToPixMap
 *
 * DESCRIPTION:	Convert a JPEG to an EmPixMap.
 *
 * PARAMETERS:	stream - stream providing the input JPEG data
 *				pixMap - pixMap to receive the uncompressed image.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void JPEGToPixMap (EmStream& stream, EmPixMap& pixMap)
{
	EmJPEGDecompressStreamSource	src_data (stream);
	EmJPEGDecompressPixMapSink		dest_data (pixMap);

	::ConvertJPEG (src_data, dest_data);
}
