using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectAuroraZetaTarget : TargetRules
{
    public ProjectAuroraZetaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("ProjectAuroraZeta");
    }
}
