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

#ifndef EmJPEG_h
#define EmJPEG_h

// jmorecfg.h tries to typedef uint32 to be a long.  BASETSD.H
// already defines uint32 to be an int.  Fortunately, they amount
// to the same thing, so we can turn off the jmorecfg.h attempt
// by defining the following symbol.

#define XMD_H 1

// jmorecfg.h also tries to define FAR.  Undefine the one set up
// by windef.h.  Yes, that gives us two different definitions of
// FAR, but hopefully that won't lead to conflict.

#undef FAR


extern "C"
{
	#include "jinclude.h"
	#include "jpeglib.h"
	#include "jerror.h"
}

class EmPixMap;
class EmStream;


// Abstract base class that acts as a source for compressed JPEG
// data that needs to be decompressed.  Clients subclass from this
// and override the pure-virtual functions.

class EmJPEGDecompressSource : public jpeg_source_mgr
{
	public:
								EmJPEGDecompressSource		(void);
		virtual					~EmJPEGDecompressSource		(void);

	private:
		virtual void			InitSource			(j_decompress_ptr cinfo) = 0;
		virtual boolean			FillInputBuffer		(j_decompress_ptr cinfo) = 0;
		virtual void			SkipInputData		(j_decompress_ptr cinfo, long num_bytes) = 0;
		virtual void			TermSource			(j_decompress_ptr cinfo) = 0;

	private:
		static void				init_source_cb		(j_decompress_ptr cinfo);
		static boolean			fill_input_buffer_cb(j_decompress_ptr cinfo);
		static void				skip_input_data_cb	(j_decompress_ptr cinfo, long num_bytes);
		static void				term_source_cb		(j_decompress_ptr cinfo);
};


// Concrete class that provides compressed JPEG data from a memory buffer.

class EmJPEGDecompressMemSource : public EmJPEGDecompressSource
{
	public:
								EmJPEGDecompressMemSource	(const void* data, long len);
		virtual					~EmJPEGDecompressMemSource	(void);

	private:
		virtual void			InitSource			(j_decompress_ptr cinfo);
		virtual boolean			FillInputBuffer		(j_decompress_ptr cinfo);
		virtual void			SkipInputData		(j_decompress_ptr cinfo, long num_bytes);
		virtual void			TermSource			(j_decompress_ptr cinfo);

	private:
		const void*				fData;
		long					fDataLen;
};


// Concrete class that provides compressed JPEG data from a stream.

class EmJPEGDecompressStreamSource : public EmJPEGDecompressSource
{
	public:
								EmJPEGDecompressStreamSource	(EmStream&);
		virtual					~EmJPEGDecompressStreamSource	(void);

	private:
		virtual void			InitSource			(j_decompress_ptr cinfo);
		virtual boolean			FillInputBuffer		(j_decompress_ptr cinfo);
		virtual void			SkipInputData		(j_decompress_ptr cinfo, long num_bytes);
		virtual void			TermSource			(j_decompress_ptr cinfo);

	private:
		EmStream&				fStream;
		void*					fBuffer;
};


// Abstract base class for accepting decompressed JPEG images and
// storing them in some client-specific fashion.  Clients subclass
// from this and override the pure-virtual functions.

class EmJPEGDecompressSink
{
	public:
								EmJPEGDecompressSink	(void);
		virtual					~EmJPEGDecompressSink	(void);

		virtual void			post_jpeg_read_header	(j_decompress_ptr cinfo) = 0;

		virtual void			start_output	(j_decompress_ptr cinfo) = 0;
		virtual void			put_pixel_rows	(j_decompress_ptr cinfo,
												 JDIMENSION rows_supplied) = 0;
		virtual void			finish_output	(j_decompress_ptr cinfo) = 0;

	public:
		// These fields are passed jpeg_read_scanlines by ConvertJPEG
		JSAMPARRAY				fBuffer;
		JDIMENSION				fBufferHeight;
};


// EmJPEGDecompressSink sub-class that writes the output as an EmPixMap.

class EmJPEGDecompressPixMapSink : public EmJPEGDecompressSink
{
	public:
								EmJPEGDecompressPixMapSink	(EmPixMap&);
		virtual					~EmJPEGDecompressPixMapSink	(void);

		virtual void			post_jpeg_read_header	(j_decompress_ptr cinfo);

		virtual void			start_output	(j_decompress_ptr cinfo);
		virtual void			put_pixel_rows	(j_decompress_ptr cinfo,
												 JDIMENSION rows_supplied);
		virtual void			finish_output	(j_decompress_ptr cinfo);

	private:
		EmPixMap&				fPixMap;

		JDIMENSION				fDataWidth;		/* JSAMPLEs per row */
		JDIMENSION				fRowWidth;		/* physical width of one row in the BMP file */
		int						fPadBytes;		/* number of padding bytes needed per row */
		JDIMENSION				fCurOutputRow;	/* next row# to write to virtual array */
};


// Function that drives the decompression process.  Clients define
// the compressed JPEG data source and the decompressed JPEG data
// output by providing the appropriate source and sink classes.

void	ConvertJPEG		(EmJPEGDecompressSource&, EmJPEGDecompressSink&);


// Utility function that converts a JPEG from the stream to the given pixmap.

void	JPEGToPixMap	(EmStream&, EmPixMap&);


#endif	/* EmJPEG_h */
