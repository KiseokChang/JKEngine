"use strict";

var __sessionBookmarkDictionary = {};
var __currentUniqueID = 0;

class SessionBookmarks
{
    get Bookmarks()
    {
        var file = this.Attributes.Target.Details.DumpFileName;
        if (!(file in __sessionBookmarkDictionary))
        {
            __sessionBookmarkDictionary[file] = new BookmarkCollection;
        }
        
        return __sessionBookmarkDictionary[file];
    }
}

class Bookmark
{
    constructor(collection, name, category, timestamp)
    {
        if (timestamp.Sequence === undefined || timestamp.Steps === undefined)
        {
            throw "Bookmark must be added at a valid timestamp";
        }
        this.__collection = collection;
        this.__name = name;
        this.__category = category;
        this.__timestamp = timestamp;
        this.UniqueID = __currentUniqueID++;
    }

    get Name()
    {
        return this.__name;
    }

    set Name(value)
    {
        this.__name = value;
    }

    get Timestamp()
    {
        return this.__timestamp;
    }

    set Timestamp(value)
    {
        this.__timestamp = value;
    }

    get Category()
    {
        return this.__category;
    }

    set Category(value)
    {
        this.__category = value;
    }

    Remove()
    {
        this.__collection.__Remove(this);
    }

    toString()
    {
        if (this.Name === undefined)
        {
            return "Bookmark: " + this.__timestamp.toString();
        }
        return "Bookmark: " + this.Name;
    }
}

class BookmarkCollection
{
    constructor()
    {
        this.__bookmarks = [];
    }
    
    AddBookmark(name, category, timestamp)
    {
        if (timestamp === undefined)
        {
            // We could add a bookmark from the wrong session if the current thread doesn't belong to this session.
            // You'd have to jump through hoops to get to there, so I'll leave that as a problem for later
            timestamp = host.currentThread.TTD.Position;
        }
        if (category === undefined)
        {
            category = "bookmark";
        }
        // It's ok for name to be undefined, we'll just use the timestamp for a display string
        this.__bookmarks.push(new Bookmark(this, name, category, timestamp));
    }

    DeleteBookmarkByID(id)
    {
        this.__bookmarks = this.__bookmarks.filter(b => b.UniqueID !== id);
    }

    __Remove(bookmark)
    {
        for( var i = 0; i < this.__bookmarks.length; i++)
        {
            if (this.__bookmarks[i] === bookmark)
            {
                this.__bookmarks.splice(i, 1);
                return;
            }
        }
    }

    *[Symbol.iterator]()
    {
        yield* this.__bookmarks;
    }

    SaveAsJson()
    {
        var serialBookmarks = this.__bookmarks.map(
            x => 
            {
                return {
                    Name: x.Name,
                    Category: x.Category,
                    Sequence: x.Timestamp.Sequence.asNumber(),
                    Steps: x.Timestamp.Steps.asNumber()
                };
            }
        )
        return JSON.stringify(serialBookmarks);
    }

    LoadFromJson(jsonBookmarks)
    {
        var create = host.namespace.Debugger.Utility.Objects.CreateInstance;
        var serialBookmarks = JSON.parse(jsonBookmarks);
        this.__bookmarks = serialBookmarks.map(
            x => new Bookmark(this, x.Name, x.Category, create("Debugger.Models.TTD.Position", x.Sequence, x.Steps))
        )
    }

    toString()
    {
        return "Bookmark collection";
    }
}

function initializeScript()
{
    return [new host.namespacePropertyParent(SessionBookmarks, "Debugger.Models.Session", "TTDAnalyze", "TTD"),
            new host.apiVersionSupport(1, 3)];
}

// SIG // Begin signature block
// SIG // MIInagYJKoZIhvcNAQcCoIInWzCCJ1cCAQExDzANBglg
// SIG // hkgBZQMEAgEFADB3BgorBgEEAYI3AgEEoGkwZzAyBgor
// SIG // BgEEAYI3AgEeMCQCAQEEEBDgyQbOONQRoqMAEEvTUJAC
// SIG // AQACAQACAQACAQACAQAwMTANBglghkgBZQMEAgEFAAQg
// SIG // 7aX/GnuE6sGgPGKORLPLKTHYvWdrVZaKCX07IVOuL/ig
// SIG // ggzJMIIGBDCCA+ygAwIBAgITMwAAAhz6zcWb6C9+xAAA
// SIG // AAACHDANBgkqhkiG9w0BAQsFADBXMQswCQYDVQQGEwJV
// SIG // UzEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9u
// SIG // MSgwJgYDVQQDEx9NaWNyb3NvZnQgQ29kZSBTaWduaW5n
// SIG // IFBDQSAyMDI0MB4XDTI2MDQxNjE4NTk0MVoXDTI3MDQx
// SIG // NTE4NTk0MVowdDELMAkGA1UEBhMCVVMxEzARBgNVBAgT
// SIG // Cldhc2hpbmd0b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAc
// SIG // BgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3JhdGlvbjEeMBwG
// SIG // A1UEAxMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMIIBIjAN
// SIG // BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1bGX4Dip
// SIG // jN9Rz36FjqDRIsNEpQoiMVDAtCPTTFm7nCjsP3vZT6AK
// SIG // HoUFbukhuuVeBD862LJwZxTzaIuPx6DnY4c9apKxLeCO
// SIG // rRHMV1OqDnmPcxr3gv94gXroS2MTNzPz5HFKHmxfjXnZ
// SIG // 5vDpHUj6A7vIplYhz0Kv/AkFLtFkUeKxPnTEX66Van5j
// SIG // Ytqlgl/eE+DLHqYoxlZMBP/7SYNK8gImHR09+C0p5Rv0
// SIG // UgWZkERlmeYPI6pyo0T2q0qjH7dYL47lE1YLVjWX4HCx
// SIG // UiuVmtJsq6vDj3IExhrEYLp/rZ0kviMQ08VbADx9Ts7z
// SIG // 48KJoLgcoVHvznL1DdA+Vpqe8QIDAQABo4IBqjCCAaYw
// SIG // DgYDVR0PAQH/BAQDAgeAMB8GA1UdJQQYMBYGCisGAQQB
// SIG // gjdMCAEGCCsGAQUFBwMDMB0GA1UdDgQWBBTaB+2tmA4z
// SIG // ksKZKegx3JlEuyftMjBUBgNVHREETTBLpEkwRzEtMCsG
// SIG // A1UECxMkTWljcm9zb2Z0IElyZWxhbmQgT3BlcmF0aW9u
// SIG // cyBMaW1pdGVkMRYwFAYDVQQFEw0yMzAwMTIrNTA3NTY5
// SIG // MB8GA1UdIwQYMBaAFH9ZP1Qh2q1P7wXl5qPXLQaUEggx
// SIG // MGAGA1UdHwRZMFcwVaBToFGGT2h0dHA6Ly93d3cubWlj
// SIG // cm9zb2Z0LmNvbS9wa2lvcHMvY3JsL01pY3Jvc29mdCUy
// SIG // MENvZGUlMjBTaWduaW5nJTIwUENBJTIwMjAyNC5jcmww
// SIG // bQYIKwYBBQUHAQEEYTBfMF0GCCsGAQUFBzAChlFodHRw
// SIG // Oi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtpb3BzL2NlcnRz
// SIG // L01pY3Jvc29mdCUyMENvZGUlMjBTaWduaW5nJTIwUENB
// SIG // JTIwMjAyNC5jcnQwDAYDVR0TAQH/BAIwADANBgkqhkiG
// SIG // 9w0BAQsFAAOCAgEAFJxKoWkV3tE94SCY73UBKxJKwP+2
// SIG // wco5+reSAKzg5JEY85GMLSjHNsmI9qrmjay7rVsNmGXJ
// SIG // 4Cj8tW+9WMgyUE8uDQ0cGkofU8ObYa5NzZnD6wB4mub7
// SIG // XASdQoLSiu5kGyHENtnfzd/Nd2sggwxXsLtfo7GZl/q/
// SIG // 2kxKmjjOE1cVbUUpLgsvJwFyrgoTii4v8wOF7h/IhGKi
// SIG // LI9mKDWnksVZnhohEV6SnaN3Q5mItJDucNg/FUuHN/vY
// SIG // eoBJWAWgAIP3WBKwYNu6k9779M0QyYSbn7wjcpQPEu//
// SIG // vB+RPz1eXJ4Op2vVVf8PTld6rrjQ+s3RmthF9/BpaedB
// SIG // fQCEJN6dsV5nL6Kw3jOFye1JVmAYuoPNCdUkjkJyJwmB
// SIG // RJrH1DZ9/tQGkySkiS/N6rigK02nNqSobtGM88686Oh6
// SIG // 7EYkCs6Z0QW9f3TGuj94c++V2zEQXLTbBYWQtO1gpoxM
// SIG // XS4Nnh1ubldE2PA+fusKMyX+7xd/lh5GDzvOWfgQulOB
// SIG // ZDW2DcnGfXBOI9bV0Xcgwn5penNB1jx4zVQzm67/ZSrd
// SIG // 6lKhaV9/FQqlQsjTjtVHF30IlYycN9lNllCmY7f53iSh
// SIG // xAbJvZBbC7ls5EOd/qnGkmsrZrAp5NoDoJa5Q+Xd5Csr
// SIG // 7wMPq85tJU/Ct/D+jy8X2UB4buFvHVewL/DdmZgwgga9
// SIG // MIIEpaADAgECAhMzAAAAOTu2Nxm/Bh1nAAAAAAA5MA0G
// SIG // CSqGSIb3DQEBDAUAMIGIMQswCQYDVQQGEwJVUzETMBEG
// SIG // A1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9u
// SIG // ZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9u
// SIG // MTIwMAYDVQQDEylNaWNyb3NvZnQgUm9vdCBDZXJ0aWZp
// SIG // Y2F0ZSBBdXRob3JpdHkgMjAxMTAeFw0yNDA4MDgyMDU0
// SIG // MThaFw0zNjAzMjIyMjEzMDRaMFcxCzAJBgNVBAYTAlVT
// SIG // MR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24x
// SIG // KDAmBgNVBAMTH01pY3Jvc29mdCBDb2RlIFNpZ25pbmcg
// SIG // UENBIDIwMjQwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAw
// SIG // ggIKAoICAQDYAZwe4zjHqpUWBzWtuub+CGPXx/EyoXph
// SIG // 3zyDXtYKS2ld3YYN9uFsB9Oi3B26Z7AbpAgzYra8qNHb
// SIG // UvxFuiP8hC/2y0mPISqW30LlrrAT6/ams2HA8Qlv6p42
// SIG // +SbCNbPGzToN21QE70FS+LXH9N2k8nLM/EHgnTNJf8h0
// SIG // TmyfUKmszNa+lTxDieyy/rhBG+98OkArobPPWtbr9c3q
// SIG // zmDJ7J3kUcAm6cltdSHIIFNHESgw6taY1ScyGyBevqIl
// SIG // 120XjrIHiPM7tRckHytH1ZGsmvEplR0P7Tn9t5meFvZN
// SIG // EYttkFvad1IEguTlA5LSscXAphi+rVy3zhklhyCFeGK0
// SIG // yU0+jzbcuURKIxybmRwK5BfVZx0xEVqE4wM3yN5D/uW+
// SIG // GpVHYYAGe7bTrtW1Z13x2qj2Jdqz7NtI4tNyzlVrIf62
// SIG // nYBNe3rOYS/repVdHlR61YbLLETlibs9jFzAre4sO5RT
// SIG // xvS1yho7JqJ59oKLRnRyLhIOSZyTCVZosXeS0ZZJoGEW
// SIG // Ss4cUgsMqBiKtD4WgO2PlT3LeaQh5Io3CCA5tJ5ZCvtC
// SIG // snqaJXKhptE/xmEETIRyZRjjplUKKd+sFFVGJJVMvvrw
// SIG // 1nhIBKOLO4cTepiG39jEiEP4iHzGYCcQuvaLpDFFwqzg
// SIG // t0pBP8SJIKX5dtjDNYrZGd+ZzV5DKJVNZQIDAQABo4IB
// SIG // TjCCAUowDgYDVR0PAQH/BAQDAgGGMBAGCSsGAQQBgjcV
// SIG // AQQDAgEAMB0GA1UdDgQWBBR/WT9UIdqtT+8F5eaj1y0G
// SIG // lBIIMTAZBgkrBgEEAYI3FAIEDB4KAFMAdQBiAEMAQTAP
// SIG // BgNVHRMBAf8EBTADAQH/MB8GA1UdIwQYMBaAFHItOgIx
// SIG // kEO5FAVO4eqnxzHRI4k0MFoGA1UdHwRTMFEwT6BNoEuG
// SIG // SWh0dHA6Ly9jcmwubWljcm9zb2Z0LmNvbS9wa2kvY3Js
// SIG // L3Byb2R1Y3RzL01pY1Jvb0NlckF1dDIwMTFfMjAxMV8w
// SIG // M18yMi5jcmwwXgYIKwYBBQUHAQEEUjBQME4GCCsGAQUF
// SIG // BzAChkJodHRwOi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtp
// SIG // L2NlcnRzL01pY1Jvb0NlckF1dDIwMTFfMjAxMV8wM18y
// SIG // Mi5jcnQwDQYJKoZIhvcNAQEMBQADggIBABSUHzgoT+6J
// SIG // 5+nyyDCq0pTdVmCsAxYAHXcpjlDtxazPHewf1v4kOg8V
// SIG // 7A5+w+VuMDMGHi8rLXBKn5I8+DVEUYGs8jLuckc0IeC6
// SIG // owOLUrU3CYdaKRMaO55+T7jwWJ27tPkx0rlR03tFU0z1
// SIG // YYpcv6Yhaw6N2sUPT+AvjpecnrftoE33pCAkucUvnGH0
// SIG // iL4J9CZLFQVTGFSOUBbv6oZy4bBBRFMxvH779IY4JDvp
// SIG // ZKVfbcuhpDeL3Z3e8mukOmkfct+GojNapsWsQYujlJ8j
// SIG // Zen5Lrp/3YkxZ2Ay06aTpK/5oOVknwog1TDQsbY+MDyg
// SIG // uTph5tQ0CLfzDaJG2x91BrBT9UG87C6HLkqiwrx9PSKN
// SIG // 3wz05rHEfWO+RuKl+0U1/AHQT6NCOjhKI39/c7hWbdKj
// SIG // h5uuWFkBOvXGTNrnhNTAdOXTTYByvYExO8yryv34PAdq
// SIG // o1vPDE/1heVebr2RramvRUi9kWswKwPqwz7n+iRmM+B6
// SIG // YDGRweEurM1kimAb9FYrAs38YHlPnarl1vW3dGrmJTge
// SIG // fAz3DmCnXN0nveIPsS+KXBIWweeCToAJMGE7v/XS3h9q
// SIG // Q6niWQAAVQ1kUAml3zuS4MisCgi2F6YoK2WAo1EgXK/l
// SIG // XvDxVjIVU0JdL+KvCfwFJkDeVuJ9dNXGNi+AOxk0BtYd
// SIG // 9hxwL30BElj9MYIZ+TCCGfUCAQEwbjBXMQswCQYDVQQG
// SIG // EwJVUzEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0
// SIG // aW9uMSgwJgYDVQQDEx9NaWNyb3NvZnQgQ29kZSBTaWdu
// SIG // aW5nIFBDQSAyMDI0AhMzAAACHPrNxZvoL37EAAAAAAIc
// SIG // MA0GCWCGSAFlAwQCAQUAoIGuMBkGCSqGSIb3DQEJAzEM
// SIG // BgorBgEEAYI3AgEEMBwGCisGAQQBgjcCAQsxDjAMBgor
// SIG // BgEEAYI3AgEVMC8GCSqGSIb3DQEJBDEiBCAZFWo2Js81
// SIG // cGsVn5fbK7skbGJI+Udvm5a6rNw+7I5qlzBCBgorBgEE
// SIG // AYI3AgEMMTQwMqAUgBIATQBpAGMAcgBvAHMAbwBmAHSh
// SIG // GoAYaHR0cDovL3d3dy5taWNyb3NvZnQuY29tMA0GCSqG
// SIG // SIb3DQEBAQUABIIBABsPkR1XPEaE01eJCPxIPeqgzijZ
// SIG // hPW/ScIGHUCX0eEjzM7DGAzAoZhH8hT4Y05i6RGd+oqx
// SIG // pGxFg5E+BufQNdHBGyeFp8RMRDQwzhqG79NA+IV1jk9r
// SIG // 5zJLWpfpB1bW3asx5cl+0+WtsFCprZpZQ7iDQPumxs9/
// SIG // Q2j5vwmAQ8+6TL0HQ/VfxAWOg/n0/ZNrd6FnsNgJRksp
// SIG // nCi9tWSTEhEnfDvNgolOEhM4V/YopCYY4fFNb/ZlmgXA
// SIG // J+kxTrDMzG2slObDuSHuIrGqrwK3Ftj7BEdb0PK7HRBn
// SIG // i52ZzW6+MGXf3WhAnyzsNuQjNabFztZ7WklWrrWGpIAd
// SIG // ZDcBbduhgherMIIXpwYKKwYBBAGCNwMDATGCF5cwgheT
// SIG // BgkqhkiG9w0BBwKggheEMIIXgAIBAzEPMA0GCWCGSAFl
// SIG // AwQCAQUAMIIBWQYLKoZIhvcNAQkQAQSgggFIBIIBRDCC
// SIG // AUACAQEGCisGAQQBhFkKAwEwMTANBglghkgBZQMEAgEF
// SIG // AAQgkBK5IGZ2VIHi2FRRWt/Pk83zAMgrOR8KGWzr5Z/g
// SIG // VPQCBmo1IPBtMBgSMjAyNjA2MjIxODA4MjIuNTVaMASA
// SIG // AgH0oIHZpIHWMIHTMQswCQYDVQQGEwJVUzETMBEGA1UE
// SIG // CBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEe
// SIG // MBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMS0w
// SIG // KwYDVQQLEyRNaWNyb3NvZnQgSXJlbGFuZCBPcGVyYXRp
// SIG // b25zIExpbWl0ZWQxJzAlBgNVBAsTHm5TaGllbGQgVFNT
// SIG // IEVTTjo2QjA1LTA1RTAtRDk0NzElMCMGA1UEAxMcTWlj
// SIG // cm9zb2Z0IFRpbWUtU3RhbXAgU2VydmljZaCCEfowggco
// SIG // MIIFEKADAgECAhMzAAACEUUYOZtDz/xsAAEAAAIRMA0G
// SIG // CSqGSIb3DQEBCwUAMHwxCzAJBgNVBAYTAlVTMRMwEQYD
// SIG // VQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25k
// SIG // MR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24x
// SIG // JjAkBgNVBAMTHU1pY3Jvc29mdCBUaW1lLVN0YW1wIFBD
// SIG // QSAyMDEwMB4XDTI1MDgxNDE4NDgxM1oXDTI2MTExMzE4
// SIG // NDgxM1owgdMxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpX
// SIG // YXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYD
// SIG // VQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xLTArBgNV
// SIG // BAsTJE1pY3Jvc29mdCBJcmVsYW5kIE9wZXJhdGlvbnMg
// SIG // TGltaXRlZDEnMCUGA1UECxMeblNoaWVsZCBUU1MgRVNO
// SIG // OjZCMDUtMDVFMC1EOTQ3MSUwIwYDVQQDExxNaWNyb3Nv
// SIG // ZnQgVGltZS1TdGFtcCBTZXJ2aWNlMIICIjANBgkqhkiG
// SIG // 9w0BAQEFAAOCAg8AMIICCgKCAgEAz7m7MxAdL5Vayrk7
// SIG // jsMo3GnhN85ktHCZEvEcj4BIccHKd/NKC7uPvpX5dhO6
// SIG // 3W6VM5iCxklG8qQeVVrPaKvj8dYYJC7DNt4NN3XlVdC/
// SIG // voveJuPPhTJ/u7X+pYmV2qehTVPOOB1/hpmt51SzgxZc
// SIG // zMdnFl+X2e1PgutSA5CAh9/Xz5NW0CxnYVz8g0Vpxg+B
// SIG // q32amktRXr8m3BSEgUs8jgWRPVzPHEczpbhloGGEfHaR
// SIG // OmHhVKIqN+JhMweEjU2NXM2W6hm32j/QH/I/KWqNNfYc
// SIG // hHaG0xJljVTYoUKPpcQDuhH9dQKEgvGxj2U5/3Fq1em4
// SIG // dO6Ih04m6R+ttxr6Y8oRJH9ZhZ3sciFBIvZh7E2YFXOj
// SIG // P4MGybSylQTPDEFAtHHgpkskeEUhsPDR9VvWWhekhQx3
// SIG // qXaAKh+AkLmz/hpE3e0y+RIKO2AREjULJAKgf+R9QnNv
// SIG // qMeMkz9PGrjsijqWGzB2k2JNyaUYKlbmQweOabsCioiY
// SIG // 2fJbimjVyFAGk5AeYddUFxvJGgRVCH7BeBPKAq7MMOmS
// SIG // CTOMZ0Sw6zyNx4Uhh5Y0uJ0ZOoTKnB3KfdN/ba/eKHFe
// SIG // Ehi3WqAfzTxiy0rMvhsfsXZK7zoclqaRvVl8Q48J174+
// SIG // eyriypY9HhU+ohgiYi4uQGDDVdTDeKDtoC/hD2Cn+ARz
// SIG // wE1rFfECAwEAAaOCAUkwggFFMB0GA1UdDgQWBBRifUUD
// SIG // wOnqIcvfb53+yV0EZn7OcDAfBgNVHSMEGDAWgBSfpxVd
// SIG // AF5iXYP05dJlpxtTNRnpcjBfBgNVHR8EWDBWMFSgUqBQ
// SIG // hk5odHRwOi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtpb3Bz
// SIG // L2NybC9NaWNyb3NvZnQlMjBUaW1lLVN0YW1wJTIwUENB
// SIG // JTIwMjAxMCgxKS5jcmwwbAYIKwYBBQUHAQEEYDBeMFwG
// SIG // CCsGAQUFBzAChlBodHRwOi8vd3d3Lm1pY3Jvc29mdC5j
// SIG // b20vcGtpb3BzL2NlcnRzL01pY3Jvc29mdCUyMFRpbWUt
// SIG // U3RhbXAlMjBQQ0ElMjAyMDEwKDEpLmNydDAMBgNVHRMB
// SIG // Af8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMIMA4G
// SIG // A1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAgEA
// SIG // pEKdnMeIIUiU6PatZ/qbrwiDzYUMKRczC4Bp/XY1S9Nm
// SIG // HI+2c3dcpwH2SOmDfdvIIqt7mRrgvBPYOvJ9CtZS5eeI
// SIG // rsObC0b0ggKTv2wrTgWG+qktqNFEhQeipdURNLN68uHA
// SIG // m5edwBytd1kwy5r6B93klxDsldOmVWtw/ngj7knN09mu
// SIG // Cmwr17JnsMFcoIN/H59s+1RYN7Vid4+7nj8FcvYy9rbZ
// SIG // OMndBzsTiosF1M+aMIJX2k3EVFVsuDL7/R5ppI9Tg7eW
// SIG // QOWKMZHPdsA3ZqWzDuhJqTzoFSQShnZenC+xq/z9BhHP
// SIG // FFbUtfjAoG6EDPjSQJYXmogja8OEa19xwnh3wVufeP+c
// SIG // k+/0gxNi7g+kO6WaOm052F4siD8xi6Uv75L7798lHvPT
// SIG // hcxHHsgXqMY592d1wUof3tL/eDaQ0UhnYCU8yGkU2XJn
// SIG // ctONnBKAvURAvf2qiIWDj4Lpcm0zA7VuofuJR1Tpuyc5
// SIG // p1ja52bNZBBVqAOwyDhAmqWsJXAjYXnssC/fJkee314F
// SIG // h+GIyMgvAPRScgqRZqV16dTBYvoe+w1n/wWs/ySTUsxD
// SIG // w4T/AITcu5PAsLnCVpArDrFLRTFyut+eHUoG6UYZfj8/
// SIG // RsuQ42INse1pb/cPm7G2lcLJtkIKT80xvB1LiaNvPTBV
// SIG // EcmNSvFUM0xrXZXcYcxVXiYwggdxMIIFWaADAgECAhMz
// SIG // AAAAFcXna54Cm0mZAAAAAAAVMA0GCSqGSIb3DQEBCwUA
// SIG // MIGIMQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGlu
// SIG // Z3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMV
// SIG // TWljcm9zb2Z0IENvcnBvcmF0aW9uMTIwMAYDVQQDEylN
// SIG // aWNyb3NvZnQgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3Jp
// SIG // dHkgMjAxMDAeFw0yMTA5MzAxODIyMjVaFw0zMDA5MzAx
// SIG // ODMyMjVaMHwxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpX
// SIG // YXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYD
// SIG // VQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xJjAkBgNV
// SIG // BAMTHU1pY3Jvc29mdCBUaW1lLVN0YW1wIFBDQSAyMDEw
// SIG // MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA
// SIG // 5OGmTOe0ciELeaLL1yR5vQ7VgtP97pwHB9KpbE51yMo1
// SIG // V/YBf2xK4OK9uT4XYDP/XE/HZveVU3Fa4n5KWv64NmeF
// SIG // RiMMtY0Tz3cywBAY6GB9alKDRLemjkZrBxTzxXb1hlDc
// SIG // wUTIcVxRMTegCjhuje3XD9gmU3w5YQJ6xKr9cmmvHaus
// SIG // 9ja+NSZk2pg7uhp7M62AW36MEBydUv626GIl3GoPz130
// SIG // /o5Tz9bshVZN7928jaTjkY+yOSxRnOlwaQ3KNi1wjjHI
// SIG // NSi947SHJMPgyY9+tVSP3PoFVZhtaDuaRr3tpK56KTes
// SIG // y+uDRedGbsoy1cCGMFxPLOJiss254o2I5JasAUq7vnGp
// SIG // F1tnYN74kpEeHT39IM9zfUGaRnXNxF803RKJ1v2lIH1+
// SIG // /NmeRd+2ci/bfV+AutuqfjbsNkz2K26oElHovwUDo9Fz
// SIG // pk03dJQcNIIP8BDyt0cY7afomXw/TNuvXsLz1dhzPUNO
// SIG // wTM5TI4CvEJoLhDqhFFG4tG9ahhaYQFzymeiXtcodgLi
// SIG // Mxhy16cg8ML6EgrXY28MyTZki1ugpoMhXV8wdJGUlNi5
// SIG // UPkLiWHzNgY1GIRH29wb0f2y1BzFa/ZcUlFdEtsluq9Q
// SIG // BXpsxREdcu+N+VLEhReTwDwV2xo3xwgVGD94q0W29R6H
// SIG // XtqPnhZyacaue7e3PmriLq0CAwEAAaOCAd0wggHZMBIG
// SIG // CSsGAQQBgjcVAQQFAgMBAAEwIwYJKwYBBAGCNxUCBBYE
// SIG // FCqnUv5kxJq+gpE8RjUpzxD/LwTuMB0GA1UdDgQWBBSf
// SIG // pxVdAF5iXYP05dJlpxtTNRnpcjBcBgNVHSAEVTBTMFEG
// SIG // DCsGAQQBgjdMg30BATBBMD8GCCsGAQUFBwIBFjNodHRw
// SIG // Oi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtpb3BzL0RvY3Mv
// SIG // UmVwb3NpdG9yeS5odG0wEwYDVR0lBAwwCgYIKwYBBQUH
// SIG // AwgwGQYJKwYBBAGCNxQCBAweCgBTAHUAYgBDAEEwCwYD
// SIG // VR0PBAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wHwYDVR0j
// SIG // BBgwFoAU1fZWy4/oolxiaNE9lJBb186aGMQwVgYDVR0f
// SIG // BE8wTTBLoEmgR4ZFaHR0cDovL2NybC5taWNyb3NvZnQu
// SIG // Y29tL3BraS9jcmwvcHJvZHVjdHMvTWljUm9vQ2VyQXV0
// SIG // XzIwMTAtMDYtMjMuY3JsMFoGCCsGAQUFBwEBBE4wTDBK
// SIG // BggrBgEFBQcwAoY+aHR0cDovL3d3dy5taWNyb3NvZnQu
// SIG // Y29tL3BraS9jZXJ0cy9NaWNSb29DZXJBdXRfMjAxMC0w
// SIG // Ni0yMy5jcnQwDQYJKoZIhvcNAQELBQADggIBAJ1Vffwq
// SIG // reEsH2cBMSRb4Z5yS/ypb+pcFLY+TkdkeLEGk5c9MTO1
// SIG // OdfCcTY/2mRsfNB1OW27DzHkwo/7bNGhlBgi7ulmZzpT
// SIG // Td2YurYeeNg2LpypglYAA7AFvonoaeC6Ce5732pvvinL
// SIG // btg/SHUB2RjebYIM9W0jVOR4U3UkV7ndn/OOPcbzaN9l
// SIG // 9qRWqveVtihVJ9AkvUCgvxm2EhIRXT0n4ECWOKz3+SmJ
// SIG // w7wXsFSFQrP8DJ6LGYnn8AtqgcKBGUIZUnWKNsIdw2Fz
// SIG // Lixre24/LAl4FOmRsqlb30mjdAy87JGA0j3mSj5mO0+7
// SIG // hvoyGtmW9I/2kQH2zsZ0/fZMcm8Qq3UwxTSwethQ/gpY
// SIG // 3UA8x1RtnWN0SCyxTkctwRQEcb9k+SS+c23Kjgm9swFX
// SIG // SVRk2XPXfx5bRAGOWhmRaw2fpCjcZxkoJLo4S5pu+yFU
// SIG // a2pFEUep8beuyOiJXk+d0tBMdrVXVAmxaQFEfnyhYWxz
// SIG // /gq77EFmPWn9y8FBSX5+k77L+DvktxW/tM4+pTFRhLy/
// SIG // AsGConsXHRWJjXD+57XQKBqJC4822rpM+Zv/Cuk0+CQ1
// SIG // ZyvgDbjmjJnW4SLq8CdCPSWU5nR0W2rRnj7tfqAxM328
// SIG // y+l7vzhwRNGQ8cirOoo6CGJ/2XBjU02N7oJtpQUQwXEG
// SIG // ahC0HVUzWLOhcGbyoYIDVTCCAj0CAQEwggEBoYHZpIHW
// SIG // MIHTMQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGlu
// SIG // Z3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMV
// SIG // TWljcm9zb2Z0IENvcnBvcmF0aW9uMS0wKwYDVQQLEyRN
// SIG // aWNyb3NvZnQgSXJlbGFuZCBPcGVyYXRpb25zIExpbWl0
// SIG // ZWQxJzAlBgNVBAsTHm5TaGllbGQgVFNTIEVTTjo2QjA1
// SIG // LTA1RTAtRDk0NzElMCMGA1UEAxMcTWljcm9zb2Z0IFRp
// SIG // bWUtU3RhbXAgU2VydmljZaIjCgEBMAcGBSsOAwIaAxUA
// SIG // Kyp8q2VdgAq1VGkzd7PZwV6zNc2ggYMwgYCkfjB8MQsw
// SIG // CQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQ
// SIG // MA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9z
// SIG // b2Z0IENvcnBvcmF0aW9uMSYwJAYDVQQDEx1NaWNyb3Nv
// SIG // ZnQgVGltZS1TdGFtcCBQQ0EgMjAxMDANBgkqhkiG9w0B
// SIG // AQsFAAIFAO3jk8wwIhgPMjAyNjA2MjIxMDU4MjBaGA8y
// SIG // MDI2MDYyMzEwNTgyMFowczA5BgorBgEEAYRZCgQBMSsw
// SIG // KTAKAgUA7eOTzAIBADAGAgEAAgFlMAcCAQACAhLPMAoC
// SIG // BQDt5OVMAgEAMDYGCisGAQQBhFkKBAIxKDAmMAwGCisG
// SIG // AQQBhFkKAwKgCjAIAgEAAgMHoSChCjAIAgEAAgMBhqAw
// SIG // DQYJKoZIhvcNAQELBQADggEBADnB6DqGy3bg+dHqPzED
// SIG // WUYUnXcbbzyOB3DiCJBLmCothJBdeOdHkS/FtApJTF88
// SIG // JeKd9SAeK5aue6PrcrgetjBnJJ/mCPrxgz03kJCyMzr6
// SIG // rysap7HAr4Q1huoAMk6bBHNHD36qa2G+PRwukaLNCuyC
// SIG // eOuXLriTAI7SF8YOJhmSHIXtAI72aLZ4UJ6ZNNmDa3JU
// SIG // sgAvDVDdxfnUlW9lHYhbXspkVm4Ae9vXER4goKbLoBmK
// SIG // Uj6FPdBN6LRHzplh+8ihHIqqtUBAstEqHpSHgnqRct7E
// SIG // vCmEascVvBdWybWyYKI58nOnU4soFaxBfmGsCAqcg7wI
// SIG // kl+CGaAWEgjaimsxggQNMIIECQIBATCBkzB8MQswCQYD
// SIG // VQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4G
// SIG // A1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0
// SIG // IENvcnBvcmF0aW9uMSYwJAYDVQQDEx1NaWNyb3NvZnQg
// SIG // VGltZS1TdGFtcCBQQ0EgMjAxMAITMwAAAhFFGDmbQ8/8
// SIG // bAABAAACETANBglghkgBZQMEAgEFAKCCAUowGgYJKoZI
// SIG // hvcNAQkDMQ0GCyqGSIb3DQEJEAEEMC8GCSqGSIb3DQEJ
// SIG // BDEiBCAxzUmOy1MFBLpzINBtOE5Xg1zog9ldGNGojm0m
// SIG // ftcTIzCB+gYLKoZIhvcNAQkQAi8xgeowgecwgeQwgb0E
// SIG // ICytM6ma74dOrVpcXC+WGMXynadQI00IRf85Ysc0Mya3
// SIG // MIGYMIGApH4wfDELMAkGA1UEBhMCVVMxEzARBgNVBAgT
// SIG // Cldhc2hpbmd0b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAc
// SIG // BgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3JhdGlvbjEmMCQG
// SIG // A1UEAxMdTWljcm9zb2Z0IFRpbWUtU3RhbXAgUENBIDIw
// SIG // MTACEzMAAAIRRRg5m0PP/GwAAQAAAhEwIgQg+QaY6BKQ
// SIG // 5CVNyMasuCswPIic4avkVrg7qbjS36GLwiowDQYJKoZI
// SIG // hvcNAQELBQAEggIAnymwkCvWl7cdt7797x1E1NrmhXF3
// SIG // jSzK0nPd2m3nGEZ8IgenEwKljYFq1BcuKYRZbSspgmfR
// SIG // gziOjFCJCgsuqOHIO+5S17IweHZ0P2twex0GnmATpdtC
// SIG // 6l/sOfCfbbWmFLD69Zg7FHgVRpfj9O7OiWFWDhxesxFJ
// SIG // KU38/9iwhvxYcUEdiiX+q8ginRP1VKaJyP8pZml4JE7L
// SIG // 1fH/VI/ZxXs399p55OGbZp8/PbPUYHBwueBMTr8wF3H1
// SIG // SVRzFMV9xzG39WEPDQcnnOJEFcc5+LneDE+croaqTind
// SIG // QQZmpDjGplkZ2efOJzQU6xVYqn8xV8OJjGZPLNiyhTt7
// SIG // YcGFc/4tBFxhg7vpZA6QB5JHCHNF3uMo9tBW02m99Fak
// SIG // 6Go9un/7apq8YRzv6N/LVFy5snaw9RomE9elezPe+6AC
// SIG // iy4L4kNrmJdfUoyNsqlKG1sbuWXJ42aV5aJ8xbeQgEIa
// SIG // Cfrrg4+nw2p8TDqhkuFNpyMfp9n73UvE+lGrIjuwLh9/
// SIG // k9fUQRHuZvjkx/KpYS9YFWfv28OFu3ugMyqDhf/Jq0my
// SIG // rwpb6dJcgLke5EGnsA11ycxUwdoAVIJDGzLbkzHnNH2h
// SIG // gJxA8m7B6tKSEAAuJs6bBciWm4KeJnOjnKr3mCMmO0PU
// SIG // ZHYlz0OrO3zxP6iXVSxdvjJ1Q/HLcNw2BaMo/Es=
// SIG // End signature block
