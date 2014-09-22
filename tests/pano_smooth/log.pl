#!/usr/bin/perl

use strict;

my $file="log.txt";
$file=shift if @ARGV;

my @val=();
my @tim=();

my $dreset=0;
my @tl=();
open FD,"<".$file;
while(<FD>){
	chomp $_;
	if($_=~/pano:/){
		$_=~s/^ +//;
		$_=~s/pano://;
		my @v=split / +/,$_;
		if($v[1]ne"-0.4999999" && !$dreset){
			$dreset=1;
			@val=();
			@tim=();
		}
		push @val,\@v;
	}elsif($_=~/^tim/){
		my @v=split / +/,$_;
		if(@tl){
#			my $v=$v[2]-$tl[2];
			my $v=($v[3]-$tl[3])*1000000+($v[4]-$tl[4]);
			my @x=($v);
			push @{$tim[$v[1]]},\@x;
		}
		@tl=@v;
	}
}
close FD;

#&tdiff();
#&verr();
#&valout();

&timsel(3);
#&timsum();
&timmean(50);
&timout();

sub valout {
	foreach my $v (@val){
		print "".(join " ",@{$v})."\n";
	}
}

sub tdiff {
	for(my $i=0;$i<@val-1;$i++){
		$val[$i]->[0]=$val[$i+1]->[0]-$val[$i]->[0];
		pop @{$val[$i]};
	}
	pop @val;
}

sub verr {
	my @v0=@{$val[0]};
	my @v1=@{$val[-1]};
	foreach my $v (@val){
		my $vc = $v0[1] + ($v->[0]-$v0[0])/($v1[0]-$v0[0])*($v1[1]-$v0[1]);
		$v->[1]-=$vc;
	}
}

sub timsel {
	my $id=shift;
	@tim=@{$tim[$id]};
}

sub timsum {
	for(my $i=1;$i<@tim;$i++){
		$tim[$i]->[0]+=$tim[$i-1]->[0];
	}
}

sub timmean {
	my $len=shift;
	$len/=2;
	for(my $i=0;$i<@tim;$i++){
		my $i1=$i-$len; $i1=0 if $i1<0;
		my $i2=$i+$len; $i2=@tim-1 if $i2>=@tim;
		for(my $j=$i1;$j<=$i2;$j++){
			$tim[$i]->[1]+=$tim[$j]->[0];
		}
		$tim[$i]->[1]/=($i2-$i1+1);
	}
}

sub timout {
	foreach my $t (@tim){
		print "".(join " ",@{$t})."\n";
	}
}
