module Lita
  module Handlers
    class Stats < Handler
      route(
        /\Astate of SV.CO for batch\s*(\d*)\s*\?*\z/i,
        :state_of_batch,
        command: true,
        restrict_to: :sv_co_team,
        help: { 'state of SV.CO for batch NUMBER?' => I18n.t('slack.help.state_of_svco') }
      )

      route(
        /\Aexpired team targets for batch\s*(\d*)\s*\?*\z/i,
        :expired_team_targets,
        command: true,
        restrict_to: :sv_co_team,
        help: { 'expired team targets for batch NUMBER?' => I18n.t('slack.help.expired_team_targets') }
      )

      route(
        /\Aexpired founder targets for batch\s*(\d*)\s*\?*\z/i,
        :expired_founder_targets,
        command: true,
        restrict_to: :sv_co_team,
        help: { 'expired founder targets for batch NUMBER?' => I18n.t('slack.help.expired_founder_targets') }
      )

      def state_of_batch(response)
        # lets avoid the need to pass response around
        @response = response

        ActiveRecord::Base.connection_pool.with_connection do
          @batch_requested = parse_batch_from_command
          @batch_requested.present? ? reply_with_state_of_batch : send_batch_missing_message
        end
      end

      def expired_team_targets(response)
        @response = response

        ActiveRecord::Base.connection_pool.with_connection do
          @batch_requested = parse_batch_from_command
          @batch_requested.present? ? reply_with_expired_team_targets : send_batch_missing_message
        end
      end

      def expired_founder_targets(response)
        @response = response

        ActiveRecord::Base.connection_pool.with_connection do
          @batch_requested = parse_batch_from_command
          @batch_requested.present? ? reply_with_expired_founder_targets : send_batch_missing_message
        end
      end

      def parse_batch_from_command
        @response.match_data[1].present? ? ::Batch.find_by_batch_number(@response.match_data[1].to_i) : nil
      end

      def send_batch_missing_message
        @response.reply I18n.t('slack.handlers.stats.batch_missing_error')
      end

      def reply_with_state_of_batch
        @response.reply batch_state_message
      end

      def reply_with_expired_team_targets
        @response.reply expired_team_targets_details
      end

      def reply_with_expired_founder_targets
        @response.reply expired_founder_targets_details
      end

      def batch_state_message
        <<~MESSAGE
          > *State of SV.CO Batch #{@batch_requested.batch_number} (#{@batch_requested.name}):*
          Total number of startups: #{total_startups_count_and_names}
          #{stage_wise_startup_counts_and_names}
          Number of inactive startups last week: #{inactive_startups_count_and_names}
          Number of startups in danger zone: #{endangered_startups_count_and_names}
          Latest deployed team targets: #{latest_deployed_targets_for(:startups)}
          Latest deployed founder targets: #{latest_deployed_targets_for(:founders)}
        MESSAGE
      end

      def total_startups_count_and_names
        names_list = list_of_startups(requested_batch_startups)
        "#{requested_batch_startups.count} (#{names_list})\n"
      end

      def requested_batch_startups
        @batch_requested.startups.not_dropped_out
      end

      def stage_wise_startup_counts_and_names
        response = ''
        stages = requested_batch_startups.pluck('DISTINCT stage')

        stages.each do |stage|
          response += 'Number of startups in _\'' + I18n.t("timeline_event.stage.#{stage}") + '\'_ stage: '
          startups = Startup.not_dropped_out.where(stage: stage, batch: @batch_requested)
          response += startups.count.to_s + " (#{list_of_startups(startups)})\n"
        end

        response
      end

      def inactive_startups_count_and_names
        startups = @batch_requested.startups.inactive_for_week
        names_list = list_of_startups(startups)
        "#{startups.count} (#{names_list})\n"
      end

      def endangered_startups_count_and_names
        startups = @batch_requested.startups.endangered
        names_list = list_of_startups(startups)
        "#{startups.count} (#{names_list})\n"
      end

      def list_of_startups(startups)
        return '' unless startups.present?
        startups.map { |startup| "<#{Rails.application.routes.url_helpers.startup_url(startup)}|#{startup.product_name}>" }.join(', ')
      end

      def expired_team_targets_details
        <<~MESSAGE
          > *Team targets expired last week for SV.CO Batch #{@batch_requested.batch_number} (#{@batch_requested.name}):*
          #{expired_team_targets_list}
        MESSAGE
      end

      def expired_team_targets_list
        # get all expired team targets for the batch
        targets = Target.for_startups_in_batch(@batch_requested).expired

        return I18n.t('slack.handlers.stats.no_expired_team_targets') unless targets.present?

        targets_list = ''
        # get all unique titles from the fetched targets - to group startups by them
        target_titles = targets.pluck(:title).uniq

        # fetch startup names for each group and append them to the response
        target_titles.each_with_index do |title, index|
          startup_ids = targets.where(title: title).pluck(:assignee_id)
          targets_list += "#{index + 1}. _#{title}_: #{list_of_startups(Startup.find(startup_ids))}\n"
        end

        targets_list
      end

      def expired_founder_targets_details
        <<~MESSAGE
        > *Founder targets expired last week for SV.CO Batch #{@batch_requested.batch_number} (#{@batch_requested.name}):*
        #{expired_founder_targets_list}
        MESSAGE
      end

      def expired_founder_targets_list
        # get all expired founder targets for the batch
        targets = Target.for_founders_in_batch(@batch_requested).expired

        return I18n.t('slack.handlers.stats.no_expired_founder_targets') unless targets.present?

        targets_list = ''
        # get all unique titles from the fetched targets - to group founders by them
        target_titles = targets.pluck(:title).uniq

        # fetch founder names for each group and append them to the response
        target_titles.each_with_index do |title, index|
          founder_ids = targets.where(title: title).pluck(:assignee_id)
          targets_list += "#{index + 1}. _#{title}_: #{list_of_founders(founder_ids)}\n"
        end

        targets_list
      end

      def list_of_founders(founder_ids)
        founder_ids.map do |founder_id|
          founder = Founder.find(founder_id)
          name = founder.fullname
          name += " (@#{founder.slack_username})" if founder.slack_username.present?
          name
        end.join(', ')
      end

      def latest_deployed_targets_for(scope)
        latest_unique_titles = fetch_latest_target_titles(scope)

        return "None\n" unless latest_unique_titles.present?

        latest_unique_titles.map do |title|
          completed_count = Target.send("for_#{scope}_in_batch", @batch_requested).where(title: title).completed.count
          pending_count = Target.send("for_#{scope}_in_batch", @batch_requested).where(title: title).pending.count
          expired_count = Target.send("for_#{scope}_in_batch", @batch_requested).where(title: title).expired.count

          "_#{title}_ (Completed: #{completed_count}, Pending: #{pending_count}, Expired: #{expired_count})"
        end.join(', ') + "\n"
      end

      def fetch_latest_target_titles(scope)
        target_titles = Target.send("for_#{scope}_in_batch", @batch_requested).order('created_at DESC').pluck(:title)

        # return the latest 5 unique titles
        target_titles.uniq[0..4]
      end
    end

    Lita.register_handler(Stats)
  end
end
