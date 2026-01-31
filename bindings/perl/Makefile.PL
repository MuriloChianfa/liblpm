#!/usr/bin/env perl
#
# Makefile.PL - Build configuration for Net::LPM
#
# This script generates the Makefile for building the Net::LPM Perl module
# which provides XS bindings to the liblpm C library.
#

use strict;
use warnings;
use ExtUtils::MakeMaker;
use Config;

# Minimum Perl version
my $MIN_PERL_VERSION = '5.010';

# Check Perl version
if ($] < 5.010) {
    die "Net::LPM requires Perl $MIN_PERL_VERSION or later (you have $])\n";
}

# Try to find liblpm
my ($lpm_inc, $lpm_lib) = find_liblpm();

print "Configuration:\n";
print "  Include path: $lpm_inc\n";
print "  Library path: $lpm_lib\n";
print "  Perl version: $]\n";
print "\n";

WriteMakefile(
    NAME             => 'Net::LPM',
    VERSION_FROM     => 'lib/Net/LPM.pm',
    ABSTRACT         => 'Perl bindings for high-performance liblpm Longest Prefix Match library',
    AUTHOR           => 'Murilo Chianfa <murilo.chianfa@outlook.com>',
    LICENSE          => 'open_source',  # Boost Software License 1.0
    MIN_PERL_VERSION => $MIN_PERL_VERSION,
    
    # Dependencies
    CONFIGURE_REQUIRES => {
        'ExtUtils::MakeMaker' => '6.64',
    },
    BUILD_REQUIRES => {
        'ExtUtils::MakeMaker' => '6.64',
    },
    PREREQ_PM => {
        'Carp'     => 0,
        'Exporter' => 0,
        'XSLoader' => 0,
    },
    TEST_REQUIRES => {
        'Test::More' => '0.98',
    },
    
    # XS configuration
    XS               => { 'LPM.xs' => 'LPM.c' },
    OBJECT           => 'LPM$(OBJ_EXT)',
    TYPEMAPS         => ['typemap'],
    
    # Compiler/Linker flags
    INC              => $lpm_inc,
    LIBS             => [$lpm_lib],
    CCFLAGS          => "$Config{ccflags} -Wall",
    
    # Clean files
    clean            => { FILES => 'Net-LPM-* *.o *.c' },
    
    # Metadata
    META_MERGE => {
        'meta-spec' => { version => 2 },
        resources => {
            repository => {
                type => 'git',
                url  => 'https://github.com/MuriloChianfa/liblpm.git',
                web  => 'https://github.com/MuriloChianfa/liblpm',
            },
            bugtracker => {
                web => 'https://github.com/MuriloChianfa/liblpm/issues',
            },
            homepage => 'https://github.com/MuriloChianfa/liblpm',
        },
        keywords => ['lpm', 'longest-prefix-match', 'routing', 'ipv4', 'ipv6', 'networking'],
    },
);

# Find liblpm installation
sub find_liblpm {
    my @inc_paths = ();
    my @lib_paths = ();
    
    # Check environment variables first
    if ($ENV{LIBLPM_INC}) {
        push @inc_paths, $ENV{LIBLPM_INC};
    }
    if ($ENV{LIBLPM_LIB}) {
        push @lib_paths, $ENV{LIBLPM_LIB};
    }
    
    # Check relative path to parent project (development)
    push @inc_paths, '../../include';
    push @lib_paths, '../../build';
    
    # Check pkg-config
    my $pkg_config_inc = `pkg-config --cflags liblpm 2>/dev/null`;
    my $pkg_config_lib = `pkg-config --libs liblpm 2>/dev/null`;
    if ($? == 0) {
        chomp($pkg_config_inc);
        chomp($pkg_config_lib);
        if ($pkg_config_inc || $pkg_config_lib) {
            return ($pkg_config_inc, $pkg_config_lib);
        }
    }
    
    # Check standard system paths
    push @inc_paths, '/usr/local/include', '/usr/include';
    push @lib_paths, '/usr/local/lib', '/usr/local/lib64', '/usr/lib', '/usr/lib64';
    
    # Find header
    my $inc_path = '';
    for my $path (@inc_paths) {
        if (-f "$path/lpm/lpm.h") {
            $inc_path = "-I$path";
            last;
        } elsif (-f "$path/lpm.h") {
            $inc_path = "-I$path";
            last;
        }
    }
    
    # Find library
    my $lib_path = '-llpm -lm';
    for my $path (@lib_paths) {
        if (-f "$path/liblpm.so" || -f "$path/liblpm.a") {
            $lib_path = "-L$path -llpm -lm";
            last;
        }
    }
    
    # Warn if not found
    unless ($inc_path) {
        warn <<"END";
WARNING: liblpm headers not found!

You need to install liblpm before building Net::LPM.

Option 1: Build from source (development):
  cd ../..
  mkdir build && cd build
  cmake ..
  make -j\$(nproc)
  sudo make install

Option 2: Install from package:
  See https://github.com/MuriloChianfa/liblpm#installation

Option 3: Set environment variables:
  LIBLPM_INC=/path/to/include LIBLPM_LIB=/path/to/lib perl Makefile.PL

END
    }
    
    return ($inc_path, $lib_path);
}

# Custom MY methods
package MY;

sub postamble {
    return <<'MAKE';

# Additional targets

# Run tests with verbose output
testv: all
	$(FULLPERLRUN) "-MExtUtils::Command::MM" "-MTest::Harness" "-e" "undef *Test::Harness::Switches; test_harness(1, '$(INST_LIB)', '$(INST_ARCHLIB)')" $(TEST_FILES)

# Run tests under valgrind for memory leak detection
valgrind: all
	PERL_DESTRUCT_LEVEL=2 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(FULLPERLRUN) -Iblib/lib -Iblib/arch -e 'use Test::Harness qw(runtests); runtests(@ARGV)' t/*.t

# Generate documentation
docs:
	pod2html lib/Net/LPM.pm > LPM.html

# Development: rebuild and test
dev: realclean all test

.PHONY: testv valgrind docs dev

MAKE
}

sub test {
    my $self = shift;
    my $test = $self->SUPER::test(@_);
    
    # Set LD_LIBRARY_PATH for tests if using development build
    $test =~ s/^(test_dynamic\s*:.*\n)/$1\tLD_LIBRARY_PATH=..\/..\/build:\$(LD_LIBRARY_PATH) \\\n/m;
    
    return $test;
}
