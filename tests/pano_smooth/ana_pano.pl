#!/usr/bin/perl

use strict;

my @npano=();
my @lpano=();
my @ltim=();

my @plot=();

open FD,"<log.txt";
while(<FD>){
	chomp $_;
	if($_=~/^pano: +(.*)$/){
		(my $gh,my $goff,my $tsdl,my $ts,my $tns)=split / /,$1;
#		$goff*=0.85;
#		my $goff=0;
		@npano=($gh,$goff,$ts,$tns);
	}
	if($_=~/^tim +5 +(.*)$/){
		(my $tsdl,my $ts,my $tns)=split / /,$1;
		if(@lpano && @ltim){
			my $tt=($ts-$ltim[0])*1e3+($tns-$ltim[1])/1e6;
			my $td=($npano[2]-$lpano[2])*1000+($npano[3]-$lpano[3])/1e6;
			my $gd=($npano[0]+$npano[1])-($lpano[0]+$lpano[1]);
			my @pl=($tt,$td,$gd);
			push @plot,\@pl;
		}
		@lpano=@npano if !@lpano;
		@ltim=($ts,$tns) if !@ltim;
	}
}
close FD;

print "#-xy\n";
print "#-xlabel ms\n";
print "#-cn swap\n";
print "#-cn panoc\n";
my @pl1=@{$plot[int(@plot*0.2)]};
my @pl2=@{$plot[int(@plot*0.8)]};
my @pld=(
	($pl2[2]-$pl1[2])/($pl2[0]-$pl1[0]),
	($pl2[2]-$pl1[2])/($pl2[1]-$pl1[1]),
);
my @plo=(
	$pl1[2]-$pl1[0]*$pld[0],
	$pl1[2]-$pl1[1]*$pld[1],
);
foreach my $pl (@plot){
	printf "%12g %12g %12g %12g\n",
		$pl->[0],
		$pl->[2]-$pl->[0]*$pld[0]-$plo[0],
		$pl->[1],
		$pl->[2]-$pl->[1]*$pld[1]-$plo[1];
}
