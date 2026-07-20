#!/usr/bin/env perl
# libvibe — Perl binding via FFI::Platypus.
#
#   cpan FFI::Platypus     # or: paru -S perl-ffi-platypus
#   VIBE_LIB=/path/libvibe.so perl vibe.pl

use strict;
use warnings;
use FFI::Platypus 2.00;

my $libpath = $ENV{VIBE_LIB} || '../../libvibe.so';
my $ffi = FFI::Platypus->new( api => 2, lib => $libpath );

$ffi->attach( vibe_version    => []                     => 'string' );
$ffi->attach( vibe_parse      => [ 'string', 'size_t', 'opaque' ] => 'opaque' );
$ffi->attach( vibe_get_string => [ 'opaque', 'string' ] => 'string' );
$ffi->attach( vibe_get_int    => [ 'opaque', 'string' ] => 'sint64' );
$ffi->attach( vibe_get_float  => [ 'opaque', 'string' ] => 'double' );
$ffi->attach( vibe_get_bool   => [ 'opaque', 'string' ] => 'bool' );
$ffi->attach( vibe_get_array  => [ 'opaque', 'string' ] => 'opaque' );
$ffi->attach( vibe_array_size => [ 'opaque' ]           => 'size_t' );
$ffi->attach( vibe_emit       => [ 'opaque' ]           => 'opaque' );
$ffi->attach( vibe_free       => [ 'opaque' ]           => 'void' );
$ffi->attach( vibe_value_free => [ 'opaque' ]           => 'void' );

my $sample = $ENV{VIBE_SAMPLE} || '../sample.vibe';
open my $fh, '<:raw', $sample or die "open $sample: $!";
local $/; my $data = <$fh>; close $fh;

my $v = vibe_parse( $data, length($data), undef );
unless ( defined $v ) { print "FAILED (perl): parse error\n"; exit 1; }

my $ok = 1;
sub check {
    my ( $name, $got, $want ) = @_;
    my $pass = ( defined $got && $got eq $want );
    $ok = 0 unless $pass;
    printf "  [%s] %s = %s\n", $pass ? "ok " : "BAD", $name, $got // "(undef)";
}

check( "version",     vibe_version(),                 "1.1.0" );
check( "name",        vibe_get_string( $v, "name" ),  "libvibe" );
check( "answer",      vibe_get_int( $v, "answer" ),   42 );
check( "pi",          sprintf( "%.5f", vibe_get_float( $v, "pi" ) ), "3.14159" );
check( "enabled",     vibe_get_bool( $v, "enabled" ) ? 1 : 0, 1 );
check( "server.host", vibe_get_string( $v, "server.host" ), "localhost" );
check( "server.port", vibe_get_int( $v, "server.port" ), 8080 );
my $arr = vibe_get_array( $v, "ports" );
check( "len(ports)",  defined $arr ? vibe_array_size($arr) : 0, 3 );

my $raw = vibe_emit($v);
my $emitted = defined $raw ? $ffi->cast( 'opaque', 'string', $raw ) : "";
if ( index( $emitted, "libvibe" ) >= 0 ) {
    print "  [ok ] emit() round-trips\n";
} else {
    $ok = 0; print "  [BAD] emit() did not round-trip\n";
}
vibe_free($raw) if defined $raw;

my $bad = vibe_parse( "name {", 6, undef );
if ( !defined $bad ) {
    print "  [ok ] rejects malformed input\n";
} else {
    $ok = 0; print "  [BAD] malformed input did not raise\n";
}

vibe_value_free($v);
print $ok ? "ALL OK (perl)\n" : "FAILED (perl)\n";
exit( $ok ? 0 : 1 );
