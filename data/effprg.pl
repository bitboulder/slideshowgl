#!/usr/bin/perl

use strict;

my $fprg=shift;

my @prg=&loadprg($fprg);
@prg=&fillprg(@prg);
my %imgs=&compileprg(@prg);
my $flst=&flstimgs(\%imgs);

#&outprg(@prg);
#&outimgs(\%imgs);
print $flst;



sub outprg {
	my @prg=@_;
	foreach my $frm (@prg){
		printf "frm %s\n",$frm->{"stay"};
		foreach my $img (@{$frm->{"imgs"}}){
			printf " img %s\n",$img->{"file"};
			printf "  pos  %s\n",join ":",@{$img->{"pos"}};
			printf "  src  %s\n",join ":",@{$img->{"src"}};
			printf "  dst  %s\n",join ":",@{$img->{"dst"}};
			printf "  on   %s\n",join ":",@{$img->{"on"}};
			printf "  off  %s\n",join ":",@{$img->{"off"}};
		}
	}
}

sub outimgs {
	my $arg=shift;
	my %imgs=%{$arg};
	foreach my $f (sort keys %imgs){
		printf "img %s\n",$f;
		foreach my $ev (@{$imgs{$f}}){
			printf "    frm %i start %s end %s src %s dst %s\n",@{$ev};
		}
	}
}

sub flstimgs {
	my $arg=shift;
	my %imgs=%{$arg};
	my $flst="";
	foreach my $f (sort keys %imgs){
		$flst.=sprintf "%s:%i",$f,@{$imgs{$f}}+0;
		foreach my $ev (@{$imgs{$f}}){
			my $str=join ":",@{$ev};
			$flst.=":".$str;
			$str=~s/[^:]//g;
			return "" if 12 != length($str);
		}
		$flst.="\n";
	}
	return $flst;
}

sub loadprg {
	my $fprg=shift;
	my $dir="";
	my $img="";
	my @prg=();
	open FD,"<".$fprg || die "open file $fprg failed";
	while(<FD>){
		chomp $_;
		$_=~s/^[[:blank:]]+|[[:blank:]]+$|#.*$//g;
		next if ""eq$_;
		(my $cmd,my $arg)=split /[[:blank:]]+/,$_,2;
		my @arg=split /:/,$arg;
		foreach(@arg){ $_=~s/^[[:blank:]]+|[[:blank:]]+//g; }
		if("dir"eq$cmd){ $dir=$arg; }
		elsif("frm"eq$cmd){
			$prg[@prg]->{"stay"}=$arg[0];
			@{$prg[-1]->{"imgs"}}=();
			@{$prg[-1]->{"evs"}}=();
		}elsif("img"eq$cmd){
			$arg=$dir."/".$arg if "/" ne substr $arg,0,1;
			$prg[-1]->{"imgs"}->[@{$prg[-1]->{"imgs"}}]->{"file"}=$arg;
			@{$prg[-1]->{"imgs"}->[-1]->{"pos"}}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"src"}}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"dst"}}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"on" }}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"off"}}=();
		}elsif($cmd=~/^(pos|src|dst|on|off)$/){
			$prg[-1]->{"imgs"}->[-1]->{$cmd}=\@arg;
		}elsif("sd"eq$cmd){
			$prg[-1]->{"imgs"}->[-1]->{"src"}=\@arg;
			$prg[-1]->{"imgs"}->[-1]->{"dst"}=\@arg;
		}
	}
	close FD;
	return @prg;
}

sub fillprg {
	my @defpos=(1,1,0,0,0);
	my @defsd =(0);
	my @defoo =(0,1);
	my @prg=@_;
	for(my $fi=0;$fi<@prg;$fi++){
		my $frm=$prg[$fi];
		$frm->{"stay"}=1 if ""eq$frm->{"stay"};
		foreach my $img (@{$frm->{"imgs"}}){
			my $prv=&findimg($fi-1,$img->{"file"},@prg);
			my $nxt=&findimg($fi+1,$img->{"file"},@prg);
			if($prv){
				&fillarg($img->{"pos"},$prv->{"dst"});
				&fillarg($img->{"src"},$prv->{"dst"});
			}else{
				&fillarg($img->{"pos"},\@defpos);
				if(@{$img->{"src"}}){
					&fillarg($img->{"src"},$img->{"pos"});
				}else{
					&fillarg($img->{"src"},\@defsd,$img->{"pos"});
				}
			}
			if($nxt){
				&fillarg($img->{"dst"},$img->{"pos"});
			}else{
				if(@{$img->{"dst"}}){
					&fillarg($img->{"dst"},$img->{"pos"});
				}else{
					&fillarg($img->{"dst"},\@defsd,$img->{"pos"});
				}
			}
			&fillarg($img->{"on" },\@defoo);
			&fillarg($img->{"off"},\@defoo);
		}
	}
	return @prg;
}

sub findimg {
	my $fi=shift;
	my $file=shift;
	my @prg=@_;
	return 0 if $fi<0 || $fi>=@prg;
	my $frm=$prg[$fi];
	foreach my $img (@{$frm->{"imgs"}}){
		return $img if $img->{"file"}eq$file;
	}
	return 0;
}

sub fillarg {
	my $dst=shift;
	while(@_){
		my $src=shift;
		for(my $i=0;$i<@{$src};$i++){
			$dst->[$i]=$src->[$i] if ""eq$dst->[$i];
		}
	}
}

sub compileprg {
	my @prg=@_;
	my %imgs=();
	for(my $fi=0;$fi<=@prg;$fi++){
		if($fi<@prg){
			foreach my $img (@{$prg[$fi]->{"imgs"}}){
				my $prv=&findimg($fi-1,$img->{"file"},@prg);
				my @path=();
				if($prv){
					&pushpath(\@path,$prv->{"off"}->[0],@{$prv->{"pos"}});
					&pushpath(\@path,$prv->{"off"}->[1],@{$prv->{"dst"}});
				}
				&pushpath(\@path,$img->{"on"}->[0],@{$img->{"src"}});
				&pushpath(\@path,$img->{"on"}->[1],@{$img->{"pos"}});
				@path=&alignpath(@path);
				&compilepath(\%imgs,$img,$fi,@path);
			}
		}
		if($fi>0){
			foreach my $prv (@{$prg[$fi-1]->{"imgs"}}){
				my $img=&findimg($fi,$prv->{"file"},@prg);
				next if $img;
				my @path=();
				&pushpath(\@path,$prv->{"off"}->[0],@{$prv->{"pos"}});
				&pushpath(\@path,$prv->{"off"}->[1],@{$prv->{"dst"}});
				@path=&alignpath(@path);
				&compilepath(\%imgs,$prv,$fi,@path);
			}
		}
	}
	return %imgs;
}

sub pushpath {
	my $path=shift;
	my $time=shift;
	my @pos=@_;
	$path->[@{$path}]->{"t"}=$time;
	$path->[      -1]->{"p"}=join ":",@pos;
}

sub alignpath {
	my @p=@_;
	my $start=$p[ 0]->{"t"};
	my $end  =$p[-1]->{"t"};
	for(my $pi=1;$pi<@p-1;$pi++){
		my $time=$p[$pi]->{"t"};
		$time="" if $time<=$start;
		$time="" if $time>=$end;
		$p[$pi]->{"t"}=$time;
	}
	for(my $pi=1;$pi<@p;$pi++){
		next if $p[$pi-1]->{"p"}ne$p[$pi]->{"p"};
		(my $pd,my $ph)=($pi,$pi-1);
		($pd,$ph)=($pi-1,$pi) if $pi==@p-1;
		$p[$ph]->{"t"}=$p[$pd]->{"t"} if ""eq$p[$ph]->{"t"};
		splice @p,$pd,1;
		$pi--;
	}
	my $p1=0;
	for(my $p2=1;$p2<@p;$p2++){
		next if ""eq$p[$p2]->{"t"};
		for(my $pi=$p1+1;$pi<$p2;$pi++){
			$p[$pi]->{"t"}=($pi-$p1)/($p2-$p1);
		}
		$p1=$p2;
	}
	return @p;
}

sub compilepath {
	my $imgs=shift;
	my $img=shift;
	my $fi=shift;
	my @path=@_;
	for(my $pi=1;$pi<@path;$pi++){
		next if $path[$pi-1]->{"p"}eq$path[$pi]->{"p"};
		my $f=$img->{"file"};
		@{$imgs->{$f}}=() if !exists $imgs->{$f};
		my @ev=&compileev($fi,$path[$pi-1],$path[$pi]);
		$imgs->{$f}->[@{$imgs->{$f}}]=\@ev;
	}
}

sub compileev {
	my $fi=shift;
	my $psrc=shift;
	my $pdst=shift;
	return ($fi,$psrc->{"t"},$pdst->{"t"},$psrc->{"p"},$pdst->{"p"});
}
