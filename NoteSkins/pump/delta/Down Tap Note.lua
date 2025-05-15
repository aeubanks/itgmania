local t = Def.ActorFrame{}


t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(diffuseshift;effectcolor1,color("#376f9b66");effectcolor2,color("#376f9b66");rotationz,-135);
}

t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(blend,Blend.Add;diffuseshift;effectcolor1,color("#cc6600FF");effectcolor2,color("#cc660066");effectclock,"bgm";effecttiming,1,0,0,0;rotationz,-135);
}

t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(diffuseshift;effectcolor1,color("#cc6600FF");effectcolor2,color("#cc6600FF");fadetop,1;rotationz,-135);
}

t[#t+1] = LoadActor("UpLeft_fill")..{
	InitCommand=cmd(blend,Blend.Add;diffuseshift;effectcolor1,color("#cc6600FF");effectcolor2,color("#cc6600FF");rotationz,-135);
}

return t