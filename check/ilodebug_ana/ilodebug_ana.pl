#!/usr/bin/perl

use strict;

my %cnt=();
my %ilo=();
while(<STDIN>){
	chomp $_;
	if($_!~/^(0x[0-9a-f]+-(eff|exif|gl|load)) *: *([A-Za-z-]+) (0x[0-9a-f]+)$/){
		print "PARSE-ERROR: $_\n";
		next;
	}
	(my $ilo,my $opt,my $typ,my $img)=($1,$2,$3,$4);
	if("set"eq$typ){
		my $cnt=exists $ilo{$ilo}->{$img}?"SS":"DS";
		$cnt{$opt}->{$cnt}++;
		$ilo{$ilo}->{$img}=1;
	}elsif("del"eq$typ){
		my $cnt=exists $ilo{$ilo}->{$img}?"SD":"DD";
		$cnt{$opt}->{$cnt}++;
		delete $ilo{$ilo}->{$img};
	}elsif("ERR-miss"eq$typ){
		$cnt{$opt}->{"MS"}++;
	}elsif("ERR-exist"eq$typ){
		$cnt{$opt}->{"EX"}++;
	}else{ print "PARSE-ERROR: $_\n"; }
}

my @cnt=("DS","SS","SD","DD","MS","EX");
my $fmt="%-4s: %6s/%-6s %6s/%-6s %6s/%-6s\n";
printf $fmt,"",@cnt;
foreach my $opt (sort keys %cnt){
	my @c=();
	foreach my $cnt (keys %{$cnt{$opt}}){
		my @ext=("","k","M","G","T");
		while($cnt{$opt}->{$cnt}>=1000){ $cnt{$opt}->{$cnt}=int($cnt{$opt}->{$cnt}/1000); shift @ext; }
		$cnt{$opt}->{$cnt}.=$ext[0];
	}
	$cnt{$opt}->{"DD"}="(".$cnt{$opt}->{"DD"}.")" if $opt=~/^(exif|load)$/ && exists $cnt{$opt}->{"DD"};
	foreach my $cnt (@cnt){ push @c,$cnt{$opt}->{$cnt}; }
	printf $fmt,$opt,@c;
}
