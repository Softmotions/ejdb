import EJDB2

public struct EJDB2Swift {

  public static var versionFull: String {
    return String(cString: ejdb_version_full());
  }

  public static var version: (major: Int, minor: Int, patch: Int) {
    return (Int(ejdb_version_major()), Int(ejdb_version_minor()), Int(ejdb_version_patch()))
  }
}

/// EJDB2 document items
public struct JBDOC {
  let id: Int64
  let json: String;

  public init(id: Int64, json: String) {
    self.id = id
    self.json = json
  }
}
