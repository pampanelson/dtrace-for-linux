#! /usr/bin/perl

# $Header:$

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;

#######################################################################
#   Command line switches.					      #
#######################################################################
my %opts;

sub main
{
	Getopt::Long::Configure('no_ignore_case');
	usage() unless GetOptions(\%opts,
		'help',
		);

	usage() if ($opts{help});
	usage() if !$ENV{BUILD_DIR};

	my $fname = "$ENV{BUILD_DIR}/port.h";
	my $inc = "";
	my $kern = "/lib/modules/$ENV{BUILD_DIR}/build";
	foreach my $f (qw{
		include/linux/cred.h
		}) {
		my $val = 0;
		if (-f "$kern/$f") {
			$val = 1;
		}
#		print "$val $kern/$f\n";
		my $name = $f;
		$name =~ s/[\/.]/_/g;
		$name = uc($name);
		$inc .= "# define HAVE_$name $val\n";
	}
	$inc = join("\n", sort(split("\n", $inc)));
	$inc = "/* This file is automatically generated via mkport.pl */\n" .
		"\n" .
		$inc . "\n";
	print $inc if $opts{v};
	###############################################
	#   Read old file.			      #
	###############################################
	my $fh = new FileHandle($fname);
	my $old = "";
	if ($fh) {
		while (<$fh>) {
			$old .= $_;
		}
		exit(0) if $old eq $inc;
	}

	if (! -d "$ENV{BUILD_DIR}") {
		mkdir("$ENV{BUILD_DIR}", 0755);
	}
	$fh = new FileHandle(">$ENV{BUILD_DIR}/port.h");
	die "Cannot create $ENV{BUILD_DIR}/port.h" if !$fh;
	print $fh $inc;
}
#######################################################################
#   Print out command line usage.				      #
#######################################################################
sub usage
{
	print <<EOF;
mkport.pl: script to set up portability workarounds
Usage: BUILD_DIR=x.y.z mkport.pl

This script setups a file called "port.h" which is needed to determine
which features or include files are available on this kernel, so we can
detect at compile time what we need to work around.

Switches:

EOF

	exit(1);
}

main();
0;

