using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectAuroraZetaEditorTarget : TargetRules
{
    public ProjectAuroraZetaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("ProjectAuroraZeta");
    }
}
