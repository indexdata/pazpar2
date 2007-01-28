#!/usr/bin/perl -w

use strict;
use ZOOM;
use XML::Simple;
use Data::Dumper;
use URI::Escape;

if (!$ARGV[0])
{
    die("Usage xx irspy-server\n");
}
my $server = $ARGV[0];

my $c = new ZOOM::Connection($server);
my $res = $c->search(new ZOOM::Query::CQL('cql.allRecords=1'));
$res->option(schema => 'zeerex');
print STDERR "Hits: " . $res->size() . "\n";
my $i;
for ($i = 0; $i < $res->size(); $i++)
{
    my $rec = $res->record($i);
    my $txt = $rec->raw();
    my $r = XML::Simple::XMLin($txt, forceArray =>
	    ['set', 'index', 'map', 'attr', 'supports', 'missing',
	    'recordSyntax', 'elementSet']);
    my $si = $r->{serverInfo};
    if ($si->{protocol} ne 'Z39.50')
    {
	next;
    }
    $si->{host} =~ s/^ *//;
    $si->{host} =~ s/ *$//;
    $si->{port} =~ s/^ *//;
    $si->{port} =~ s/ *$//;
    $si->{database} =~ s/^ *//;
    $si->{database} =~ s/ *$//;
    my $id = $si->{host} . ":" . $si->{port} . "/" . $si->{database};
    $id = uri_escape($id);
    print STDERR $id . "\n";
    open O, ">records/$id" or die "$id: $!";
    print O $txt;
    close O;
}
