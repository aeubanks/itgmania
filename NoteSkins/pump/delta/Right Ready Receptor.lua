return NOTESKIN:LoadActor("UpLeft", "Ready Receptor")..{
	Frames = {
		{ Frame = 0 };
		{ Frame = 1 };
		{ Frame = 2 };
	};
	InitCommand=cmd(rotationz,135);
};