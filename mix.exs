defmodule NervesSystemBravo.MixProject do
  use Mix.Project

  @app :nerves_system_bravo
  @version Path.join(__DIR__, "VERSION")
           |> File.read!()
           |> String.trim()

  def project do
    [
      app: @app,
      version: @version,
      elixir: "~> 1.17",
      compilers: Mix.compilers() ++ [:nerves_package],
      nerves_package: nerves_package(),
      description: "Nerves System - HTC Desire (bravo, Qualcomm QSD8250), stock Android kernel, musl",
      deps: deps(),
      aliases: [loadconfig: [&bootstrap/1]]
    ]
  end

  def application, do: []

  defp bootstrap(args) do
    set_target()
    Application.start(:nerves_bootstrap)
    Mix.Task.run("loadconfig", args)
  end

  defp nerves_package do
    [
      type: :system,
      # Never published; always built locally.
      artifact_sites: [],
      build_runner_opts: [make_args: ["source", "all", "legal-info"]],
      platform: Nerves.System.BR,
      platform_config: [defconfig: "nerves_defconfig"],
      env: [
        {"TARGET_ARCH", "armv7"},
        {"TARGET_CPU", "cortex_a8"},
        {"TARGET_OS", "linux"},
        {"TARGET_ABI", "musleabihf"},
        {"TARGET_GCC_FLAGS",
         "-mabi=aapcs-linux -mfpu=neon -marm -fstack-protector-strong -mfloat-abi=hard -mcpu=cortex-a8 -fPIE -pie -Wl,-z,now -Wl,-z,relro"}
      ],
      checksum: [
        "scripts",
        "board",
        "rootfs_overlay",
        "busybox.fragment",
        "fwup.conf",
        "mix.exs",
        "nerves_defconfig",
        "post-build.sh",
        "post-createfs.sh",
        "VERSION"
      ]
    ]
  end

  defp deps do
    [
      {:nerves, "~> 1.11 or ~> 2.0 or ~> 2.0.0-dev", runtime: false},
      {:nerves_system_br, "1.34.4", runtime: false},
      {:nerves_toolchain_bravo, path: "../nerves_toolchain_bravo", runtime: false}
    ]
  end

  defp set_target do
    if function_exported?(Mix, :target, 1) do
      apply(Mix, :target, [:target])
    else
      System.put_env("MIX_TARGET", "target")
    end
  end
end
