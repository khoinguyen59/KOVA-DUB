#pragma once

namespace LAStudio {

bool isLiveWorkflowInvocation(int argc, char *argv[]);
int runLiveWorkflow(int argc, char *argv[]);

bool isLiveDubbingStudioInvocation(int argc, char *argv[]);
int runLiveDubbingStudio(int argc, char *argv[]);

} // namespace LAStudio
