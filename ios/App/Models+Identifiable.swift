import Foundation

extension ImUser: Identifiable {}
extension ImGroup: Identifiable {}
extension ImMessage: Identifiable {}

extension ImBuddy: Identifiable {
    public var id: Int64 { user.id }
}

extension ImMember: Identifiable {
    public var id: Int64 { user.id }
}

extension ImSession: Identifiable {
    public var id: Int64 { targetId }
}
