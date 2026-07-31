import SwiftUI

@main
struct IHaveIssuesApp: App {
    var body: some Scene {
        DocumentGroup(newDocument: IssuesDocument()) { file in
            ContentView(document: file.$document)
        }
        .commands {
            IssuesCommands()
        }
    }
}
