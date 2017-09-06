# -*- mode: Perl; tab-width: 4 -*-

use File::Basename;

$language = "ENGLISH";

$input = $ARGV[0];
$output = $ARGV[1];

if (not @ARGV or not -f $input) {
	die "Usage: $0 <full path of Strings.txt> <full path to output dir>\n";
}


$output_rez = $output . "Strings.cpp";
$output_cpp = $output . "ResStrings.cpp";
$output_vcpp = $output . "Strings.rc";


@modes = (
{
	output => $output_rez,
	
	maccreator => 'CWIE',

	prefix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#/* This file is generated from SrcShared:Strings.txt */
#
#type 'STRA' {
#	integer = $$CountOf(SomeArray);
#	wide array SomeArray {
#		integer;
#		wstring;
#	};
#};
#
#resource 'STRA' (1000) {{
-EOT-

	suffix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#}};
#
-EOT-

	print => sub { "\t$_[0], \"" . backslash_quotes($_[1]) . "\";" },

	prefix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#/* This file is generated from SrcShared/Strings.txt */
#
##include <map>
##include <cstddef>
#
#static std::map<int, const char*> _ResStrMap;
#
#struct item
#{
#	int key;
#	const char* value;
#};
#
##pragma mpwc_newline on
#
#static const struct item _ResStrTable[] = {
-EOT-

	suffix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#	{ 0, NULL }
#};
#
#static void _ResStrTableInit (void)
#{
#	for (const struct item* it = _ResStrTable; it->value != NULL; it++)
#	{
#		_ResStrMap[it->key] = it->value;
#	}
#}
#
#
#const char* _ResGetString(int idx);
#const char* _ResGetString(int idx)
#{
#	static int inited;
#	if (!inited)
#	{
#		inited = 1;
#		_ResStrTableInit();
#	}
#
#	return _ResStrMap[idx];
#}
-EOT-

	print => sub { "\t{ $_[0], \"" . backslash_quotes($_[1]) . "\" }," }
},

{
	output => $output_cpp,

	prefix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#/* This file is generated from SrcShared/Strings.txt */
#
##include <map>
##include <cstddef>
#
#static std::map<int, const char*> _ResStrMap;
#
#struct item
#{
#	int key;
#	const char* value;
#};
#
#static const struct item _ResStrTable[] = {
-EOT-

	suffix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#	{ 0, NULL }
#};
#
#static void _ResStrTableInit (void)
#{
#	for (const struct item* it = _ResStrTable; it->value != NULL; it++)
#	{
#		_ResStrMap[it->key] = it->value;
#	}
#}
#
#
#const char* _ResGetString(int idx)
#{
#	static int inited;
#	if (!inited)
#	{
#		inited = 1;
#		_ResStrTableInit();
#	}
#
#	return _ResStrMap[idx];
#}
-EOT-

	print => sub { "\t{ $_[0], \"" . backslash_quotes(translate_high_ascii(1, $_[1])) . "\" }," }
},

{
	output => $output_vcpp,

	prefix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#/* This file is generated from SrcShared/Strings.txt */
#
#STRINGTABLE DISCARDABLE
#BEGIN
-EOT-

	suffix => sub { local($_); $_=<<'-EOT-'; s/^#//gm; $_ },
#END
-EOT-

	print => sub { "\t$_[0]\t\"" . double_quotes(translate_high_ascii(0, $_[1])) . "\"" }
}
);

foreach $mode (@modes) {

	open (I, "<$input") or die "Unable to open $input: $!\n";
	open (O, ">$mode->{output}") or next;

	$text = "";
	$started = 0;
	$id = "";
	$text = "";

	while (<I>) {
		if (/^\t(.+)/ and $text) {
			$text .= $1;
			next;
		}

		print O &{$mode->{print}}($id,$text) if $text;
		$text = "";
		
		if (/^START$/) {
			print O &{$mode->{prefix}}();
			$started = 1;
		} elsif (/^END$/) {
			print O &{$mode->{suffix}}();
			last;
		} elsif ($started and /^([A-Z]+)=(.+)/) {
			$id = $2 if $1 eq "ID";
			$text = $2 if $1 eq $language;
		} else {
			print O;
		}
	}

	if ($^O =~ /MacOS/i and $mode->{maccreator}) {
		MacPerl::SetFileInfo($mode->{maccreator}, 'TEXT', $mode->{output});
	}

	close (O);
	close (I);
}


sub backslash_quotes
{
	local($_)=@_;
	s/\"/\\\"/g;		# Turn a " into \"
	return $_;
}

sub double_quotes
{
	local($_)=@_;
	s/\"/\"\"/g;		# Turn a " into ""
	return $_;
}

sub translate_high_ascii
{
	local($unix, $_) = @_;
	# Translate Macintosh High Ascii characters
	# into the equivalents for Windows and Unix.

	# (Note: I tried turning the curly quotes into
	#  the equivalent on Windows, but they just looked
	#  really bad in dialogs, so I now turn them into
	#  straight quotes.)

	s/Ó/\"/g;			# Turn Ó into "
	s/Ò/\"/g;			# Turn Ò into "
	s/Õ/\'/g;			# Turn Õ into '
	s/Ô/\'/g;			# Turn Ô into '
	s/É/\.\.\./g;		# Turn É into ...
	s/ª/\(TM)/g;		# Turn ª into (TM)

	if (!$unix)
	{
		s/¨/\®/g;			# Turn ¨ into ®
	}
	else
	{
		s/¨/(R)/g;			# Turn ¨ into (R)
		s/©/(c)/g;			# Turn © into (c)
	}
	
	return $_;
}


